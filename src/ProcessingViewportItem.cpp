#include "ProcessingViewportItem.h"
#include "ProcessingController.h"
#include "ImageBuffer.h"
#include <QSGRenderNode>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <algorithm>
#include <memory>
#include <limits>

// ============================================================================
// GLSL Shader Sources (embedded as static strings)
// ============================================================================

static const char *vertShaderSrc = R"glsl(
#version 330 core
in vec2 a_pos;
out vec2 v_pos;
uniform mat4 u_mvp;
void main() {
    v_pos = a_pos;
    gl_Position = u_mvp * vec4(a_pos, 0.0, 1.0);
}
)glsl";

static const char *fragShaderSrc = R"glsl(
#version 330 core

uniform sampler2D u_texOut;
uniform sampler2D u_texIn;
uniform vec2  u_texSize;
uniform vec4  u_contentRect;   // x, y, w, h in viewport pixels
uniform float u_scale;
uniform int   u_splitCompare;
uniform float u_splitPos;      // viewport pixel X for split line
uniform int   u_showOriginal;
uniform vec4  u_bgColor;

in vec2 v_pos;
out vec4 fragColor;

// ============================================================================
// Helper Functions
// ============================================================================

vec2 clampTC(vec2 tc, vec2 invSize) {
    return clamp(tc, 0.5 * invSize, 1.0 - 0.5 * invSize);
}

// ============================================================================
// Bilinear Interpolation (manual, since texture is GL_NEAREST)
// ============================================================================

vec4 sampleBilinear(sampler2D tex, vec2 uv, vec2 invSize) {
    vec2 p = uv / invSize - 0.5;
    vec2 f = fract(p);
    vec2 base = (floor(p) + 0.5) * invSize;

    vec4 s00 = texture(tex, clampTC(base, invSize));
    vec4 s10 = texture(tex, clampTC(base + vec2(invSize.x, 0.0), invSize));
    vec4 s01 = texture(tex, clampTC(base + vec2(0.0, invSize.y), invSize));
    vec4 s11 = texture(tex, clampTC(base + invSize, invSize));

    return mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
}

// ============================================================================
// Catmull-Rom Cubic Interpolation (a = -0.5)
// ============================================================================

float cubicCR(float x) {
    float ax = abs(x);
    if (ax < 1.0) return 1.5*ax*ax*ax - 2.5*ax*ax + 1.0;
    if (ax < 2.0) return -0.5*ax*ax*ax + 2.5*ax*ax - 4.0*ax + 2.0;
    return 0.0;
}

vec4 sampleBicubic(sampler2D tex, vec2 uv, vec2 invSize) {
    vec2 p = uv / invSize - 0.5;
    vec2 f = fract(p);
    vec2 base = floor(p);

    vec4 color = vec4(0.0);
    for (int j = -1; j <= 2; j++) {
        float wy = cubicCR(float(j) - f.y);
        for (int i = -1; i <= 2; i++) {
            float w = cubicCR(float(i) - f.x) * wy;
            vec2 coord = (base + vec2(float(i), float(j)) + 0.5) * invSize;
            color += w * texture(tex, clampTC(coord, invSize));
        }
    }
    return color;
}

// ============================================================================
// Lanczos r=3 Kernel
// ============================================================================

float sinc(float x) {
    if (abs(x) < 1e-5) return 1.0;
    float px = 3.14159265358979 * x;
    return sin(px) / px;
}

float lanczos3(float x) {
    if (abs(x) >= 3.0) return 0.0;
    return sinc(x) * sinc(x / 3.0);
}

vec4 sampleLanczos3(sampler2D tex, vec2 uv, vec2 invSize) {
    vec2 p = uv / invSize - 0.5;
    vec2 f = fract(p);
    vec2 base = floor(p);

    vec4 color = vec4(0.0);
    float totalW = 0.0;

    for (int j = -2; j <= 3; j++) {
        float wy = lanczos3(float(j) - f.y);
        for (int i = -2; i <= 3; i++) {
            float w = lanczos3(float(i) - f.x) * wy;
            vec2 coord = (base + vec2(float(i), float(j)) + 0.5) * invSize;
            color += w * texture(tex, clampTC(coord, invSize));
            totalW += w;
        }
    }

    return totalW > 1e-6 ? color / totalW : vec4(0.0);
}

// ============================================================================
// Sobel Edge Detection
// ============================================================================

float sobelEdge(sampler2D tex, vec2 uv, vec2 invSize) {
    float lum[9];
    int k = 0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec3 col = texture(tex, clampTC(uv + vec2(float(i), float(j)) * invSize, invSize)).rgb;
            lum[k++] = dot(col, vec3(0.299, 0.587, 0.114));
        }
    }

    float gx = -lum[0] - 2.0*lum[3] - lum[6] + lum[2] + 2.0*lum[5] + lum[8];
    float gy = -lum[0] - 2.0*lum[1] - lum[2] + lum[6] + 2.0*lum[7] + lum[8];

    return clamp(sqrt(gx*gx + gy*gy), 0.0, 1.0);
}

// ============================================================================
// D10 Hybrid Resampling Dispatch
// ============================================================================

vec4 hybridSample(sampler2D tex, vec2 uv, vec2 invSize, float scale) {
    float edge = sobelEdge(tex, uv, invSize);
    bool isEdge = edge > 0.20;

    if (scale >= 1.0) {
        // Upscale: Bicubic for edges, Bilinear for flat regions
        return isEdge ? sampleBicubic(tex, uv, invSize) : sampleBilinear(tex, uv, invSize);
    } else {
        // Downscale: Lanczos3 for edges, Bilinear for flat regions
        return isEdge ? sampleLanczos3(tex, uv, invSize) : sampleBilinear(tex, uv, invSize);
    }
}

// ============================================================================
// Main Fragment Shader
// ============================================================================

void main() {
    float rx = u_contentRect.x, ry = u_contentRect.y;
    float rw = u_contentRect.z, rh = u_contentRect.w;

    // Letterbox: outside contentRect shows background
    if (v_pos.x < rx || v_pos.x >= rx + rw || v_pos.y < ry || v_pos.y >= ry + rh) {
        fragColor = u_bgColor;
        return;
    }

    // Normalized UV within contentRect
    vec2 uv = vec2((v_pos.x - rx) / rw, (v_pos.y - ry) / rh);
    vec2 invSize = 1.0 / u_texSize;

    vec4 color;
    if (u_splitCompare == 1) {
        // Split compare: left=original, right=result
        if (v_pos.x < u_splitPos)
            color = hybridSample(u_texIn,  uv, invSize, u_scale);
        else
            color = hybridSample(u_texOut, uv, invSize, u_scale);
    } else if (u_showOriginal == 1) {
        // Show original image
        color = hybridSample(u_texIn, uv, invSize, u_scale);
    } else {
        // Show result image
        color = hybridSample(u_texOut, uv, invSize, u_scale);
    }

    fragColor = vec4(mix(u_bgColor.rgb, color.rgb, clamp(color.a, 0.0, 1.0)), 1.0);
}
)glsl";

// ============================================================================
// Snapshot Structure (intermediate state passed to render node)
// ============================================================================

struct Snapshot {
    std::shared_ptr<ImageBuffer> outImage;
    std::shared_ptr<ImageBuffer> inImage;
    quint64 outVer = std::numeric_limits<quint64>::max();
    quint64 inVer  = std::numeric_limits<quint64>::max();
    QSizeF  viewSize;
    bool    showOriginal  = false;
    bool    splitCompare  = false;
    float   splitPosition = 0.5f;
    float   zoomScale     = 1.0f;
    float   panOffsetX    = 0.0f;
    float   panOffsetY    = 0.0f;
    QQuickWindow *window  = nullptr;  // for beginExternalCommands()
};

// ============================================================================
// ViewportRenderNode (internal to this file)
// ============================================================================

class ViewportRenderNode : public QSGRenderNode {
public:
    ViewportRenderNode() = default;
    ~ViewportRenderNode() override { releaseResources(); }

    void setSnapshot(const Snapshot &s) { pending_ = s; }
    bool hasShaderFailed() const { return shaderFailed_; }
    QString shaderError() const { return shaderError_; }

    StateFlags changedStates() const override {
        return DepthState | BlendState | ViewportState | ScissorState;
    }

    RenderingFlags flags() const override {
        return BoundedRectRendering;
    }

    QRectF rect() const override {
        return QRectF(0, 0, pending_.viewSize.width(), pending_.viewSize.height());
    }

    void releaseResources() override;
    void render(const RenderState *state) override;

private:
    bool initGL();

    QOpenGLFunctions_3_3_Core gl_;
    QOpenGLShaderProgram *shader_ = nullptr;
    GLuint vao_ = 0, vbo_ = 0;
    GLuint texOut_ = 0, texIn_ = 0;
    bool   glInitialized_ = false;
    bool   shaderFailed_  = false;
    QString shaderError_;

    Snapshot pending_;
    quint64 uploadedOutVer_ = std::numeric_limits<quint64>::max();
    quint64 uploadedInVer_  = std::numeric_limits<quint64>::max();
    QSizeF  lastViewSize_;
};

// ============================================================================
// ViewportRenderNode Implementation
// ============================================================================

bool ViewportRenderNode::initGL() {
    if (!gl_.initializeOpenGLFunctions()) {
        shaderFailed_ = true;
        shaderError_ = "Failed to initialize OpenGL 3.3 Core functions";
        return false;
    }

    // Create and compile shader program
    shader_ = new QOpenGLShaderProgram;
    if (!shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertShaderSrc)) {
        shaderError_ = "Vertex shader compile failed: " + shader_->log();
        shaderFailed_ = true;
        delete shader_;
        shader_ = nullptr;
        return false;
    }

    if (!shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragShaderSrc)) {
        shaderError_ = "Fragment shader compile failed: " + shader_->log();
        shaderFailed_ = true;
        delete shader_;
        shader_ = nullptr;
        return false;
    }

    if (!shader_->link()) {
        shaderError_ = "Shader link failed: " + shader_->log();
        shaderFailed_ = true;
        delete shader_;
        shader_ = nullptr;
        return false;
    }

    // Create VAO and VBO for full-screen quad
    gl_.glGenVertexArrays(1, &vao_);
    gl_.glGenBuffers(1, &vbo_);
    gl_.glBindVertexArray(vao_);
    gl_.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_.glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    int loc = shader_->attributeLocation("a_pos");
    gl_.glEnableVertexAttribArray(loc);
    gl_.glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    gl_.glBindVertexArray(0);

    // Create textures for output and input images
    auto setupTex = [&](GLuint &tex) {
        gl_.glGenTextures(1, &tex);
        gl_.glBindTexture(GL_TEXTURE_2D, tex);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    setupTex(texOut_);
    setupTex(texIn_);

    glInitialized_ = true;
    return true;
}

void ViewportRenderNode::releaseResources() {
    if (!glInitialized_) return;

    if (texOut_) gl_.glDeleteTextures(1, &texOut_);
    if (texIn_)  gl_.glDeleteTextures(1, &texIn_);
    if (vbo_)    gl_.glDeleteBuffers(1, &vbo_);
    if (vao_)    gl_.glDeleteVertexArrays(1, &vao_);

    delete shader_;
    shader_ = nullptr;

    texOut_ = texIn_ = vbo_ = vao_ = 0;
    glInitialized_ = false;
}

void ViewportRenderNode::render(const RenderState *state) {
    QQuickWindow *win = pending_.window;
    if (win) win->beginExternalCommands();

    // Lazy initialization of OpenGL resources
    if (!glInitialized_ && !shaderFailed_)
        initGL();

    // Bail if shader failed or GL not ready or no image to display
    if (shaderFailed_ || !glInitialized_ || !pending_.outImage) {
        if (win) win->endExternalCommands();
        return;
    }

    // Upload textures if version changed
    auto uploadTex = [&](GLuint tex, const std::shared_ptr<ImageBuffer> &img,
                          quint64 &uploadedVer, quint64 newVer) {
        if (!img || newVer == uploadedVer) return;

        gl_.glBindTexture(GL_TEXTURE_2D, tex);
        gl_.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        const bool hasAlpha = img->channels == 4;
        GLenum format = hasAlpha ? GL_RGBA : GL_RGB;
        GLenum internalFormat = hasAlpha ? GL_RGBA8 : GL_RGB8;
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                         img->width, img->height, 0,
                         format, GL_UNSIGNED_BYTE, img->data.data());
        uploadedVer = newVer;
    };

    uploadTex(texOut_, pending_.outImage, uploadedOutVer_, pending_.outVer);
    uploadTex(texIn_,  pending_.inImage,  uploadedInVer_,  pending_.inVer);

    // Update quad VBO if viewport size changed
    float vw = (float)pending_.viewSize.width();
    float vh = (float)pending_.viewSize.height();

    if (pending_.viewSize != lastViewSize_) {
        float verts[] = {
            0.f, 0.f,
            vw,  0.f,
            0.f, vh,
            vw,  vh
        };
        gl_.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        gl_.glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
        lastViewSize_ = pending_.viewSize;
    }

    // Calculate content rectangle and effective scale
    float sw = (float)pending_.outImage->width;
    float sh = (float)pending_.outImage->height;
    float baseScale = std::min(vw / sw, vh / sh);
    float eff = baseScale * pending_.zoomScale;
    float rw = sw * eff;
    float rh = sh * eff;
    float rx = (vw - rw) * 0.5f - pending_.panOffsetX * eff;
    float ry = (vh - rh) * 0.5f - pending_.panOffsetY * eff;

    // Set up MVP matrix
    QMatrix4x4 mvp = *state->projectionMatrix() * *matrix();

    // Background color: #F8F8F5 (warm white)
    QVector4D bgColor(0.973f, 0.973f, 0.961f, 1.0f);

    // Bind shader and set uniforms
    shader_->bind();
    shader_->setUniformValue("u_mvp", mvp);
    shader_->setUniformValue("u_texSize", QVector2D(sw, sh));
    shader_->setUniformValue("u_contentRect", QVector4D(rx, ry, rw, rh));
    shader_->setUniformValue("u_scale", eff);
    shader_->setUniformValue("u_splitCompare", pending_.splitCompare ? 1 : 0);
    shader_->setUniformValue("u_splitPos", pending_.splitPosition * vw);
    shader_->setUniformValue("u_showOriginal", pending_.showOriginal ? 1 : 0);
    shader_->setUniformValue("u_bgColor", bgColor);

    gl_.glActiveTexture(GL_TEXTURE0);
    gl_.glBindTexture(GL_TEXTURE_2D, texOut_);
    shader_->setUniformValue("u_texOut", 0);

    gl_.glActiveTexture(GL_TEXTURE1);
    gl_.glBindTexture(GL_TEXTURE_2D, texIn_);
    shader_->setUniformValue("u_texIn", 1);

    // Disable depth and blending
    gl_.glDisable(GL_DEPTH_TEST);
    gl_.glDisable(GL_BLEND);

    // Draw quad
    gl_.glBindVertexArray(vao_);
    gl_.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_.glBindVertexArray(0);

    shader_->release();
    if (win) win->endExternalCommands();
}

// ============================================================================
// ProcessingViewportItem Implementation
// ============================================================================

ProcessingViewportItem::ProcessingViewportItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(false);
}

ProcessingController *ProcessingViewportItem::controller() const {
    return controller_.data();
}

void ProcessingViewportItem::setController(ProcessingController *ctrl) {
    if (controller_ == ctrl) return;

    if (controller_) {
        disconnect(controller_, &ProcessingController::imageChanged,
                   this, &ProcessingViewportItem::onImageChanged);
        disconnect(controller_, &ProcessingController::sourceChanged,
                   this, &ProcessingViewportItem::onImageChanged);
    }

    controller_ = ctrl;
    uploadedOutVersion_ = std::numeric_limits<quint64>::max();
    uploadedInVersion_  = std::numeric_limits<quint64>::max();

    if (controller_) {
        connect(controller_, &ProcessingController::imageChanged,
                this, &ProcessingViewportItem::onImageChanged);
        connect(controller_, &ProcessingController::sourceChanged,
                this, &ProcessingViewportItem::onImageChanged);
    }

    emit controllerChanged();
    update();
}

bool ProcessingViewportItem::showOriginal() const {
    return showOriginal_;
}

void ProcessingViewportItem::setShowOriginal(bool v) {
    if (showOriginal_ == v) return;
    showOriginal_ = v;
    emit showOriginalChanged();
    update();
}

bool ProcessingViewportItem::splitCompare() const {
    return splitCompare_;
}

void ProcessingViewportItem::setSplitCompare(bool v) {
    if (splitCompare_ == v) return;
    splitCompare_ = v;
    emit splitCompareChanged();
    update();
}

qreal ProcessingViewportItem::splitPosition() const {
    return splitPosition_;
}

void ProcessingViewportItem::setSplitPosition(qreal v) {
    qreal clamped = std::clamp(v, 0.0, 1.0);
    if (splitPosition_ == clamped) return;
    splitPosition_ = clamped;
    emit splitPositionChanged();
    update();
}

qreal ProcessingViewportItem::zoomScale() const {
    return zoomScale_;
}

void ProcessingViewportItem::setZoomScale(qreal v) {
    qreal clamped = std::clamp(v, 0.1, 32.0);
    if (zoomScale_ == clamped) return;
    zoomScale_ = clamped;
    emit zoomScaleChanged();
    update();
}

qreal ProcessingViewportItem::panOffsetX() const {
    return panOffsetX_;
}

void ProcessingViewportItem::setPanOffsetX(qreal v) {
    if (panOffsetX_ == v) return;
    panOffsetX_ = v;
    emit panOffsetChanged();
    update();
}

qreal ProcessingViewportItem::panOffsetY() const {
    return panOffsetY_;
}

void ProcessingViewportItem::setPanOffsetY(qreal v) {
    if (panOffsetY_ == v) return;
    panOffsetY_ = v;
    emit panOffsetChanged();
    update();
}

void ProcessingViewportItem::resetView() {
    zoomScale_ = 1.0;
    emit zoomScaleChanged();
    panOffsetX_ = 0.0;
    panOffsetY_ = 0.0;
    emit panOffsetChanged();
    update();
}

QSGNode *ProcessingViewportItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
    auto *node = static_cast<ViewportRenderNode *>(old);

    if (!controller_ || !controller_->outImage()) {
        delete node;
        return nullptr;
    }

    if (!node)
        node = new ViewportRenderNode;

    // Check for shader failure (D11: fatal, no fallback)
    if (node->hasShaderFailed()) {
        QString err = node->shaderError();
        delete node;
        emit renderErrorOccurred("Display shader error: " + err);
        return nullptr;
    }

    // Populate snapshot for render node
    Snapshot snap;
    snap.outImage      = controller_->outImage();
    snap.inImage       = controller_->inImage();
    snap.outVer        = controller_->outImageVersion();
    snap.inVer         = controller_->inImageVersion();
    snap.viewSize      = QSizeF(width(), height());
    snap.showOriginal  = showOriginal_;
    snap.splitCompare  = splitCompare_;
    snap.splitPosition = (float)splitPosition_;
    snap.zoomScale     = (float)zoomScale_;
    snap.panOffsetX    = (float)panOffsetX_;
    snap.panOffsetY    = (float)panOffsetY_;
    snap.window        = window();

    node->setSnapshot(snap);
    return node;
}

void ProcessingViewportItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

void ProcessingViewportItem::wheelEvent(QWheelEvent *event) {
    const qreal factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    setZoomScale(std::clamp(zoomScale_ * factor, 0.1, 32.0));
    event->accept();
}

void ProcessingViewportItem::mousePressEvent(QMouseEvent *event) {
    dragging_ = true;
    dragStart_ = event->position();
    dragStartPanX_ = panOffsetX_;
    dragStartPanY_ = panOffsetY_;
    event->accept();
}

void ProcessingViewportItem::mouseMoveEvent(QMouseEvent *event) {
    if (!dragging_) return;

    if (splitCompare_) {
        // Drag updates split position
        qreal newSplitPos = event->position().x() / width();
        setSplitPosition(std::clamp(newSplitPos, 0.0, 1.0));
    } else {
        // Drag pans the image (screen pixels → source pixels)
        if (!controller_ || !controller_->outImage()) {
            event->accept();
            return;
        }

        qreal sw = (qreal)controller_->outImage()->width;
        qreal sh = (qreal)controller_->outImage()->height;
        qreal vw = width();
        qreal vh = height();
        qreal base = std::min(vw / sw, vh / sh);
        qreal eff = base * zoomScale_;

        if (eff > 0) {
            QPointF delta = event->position() - dragStart_;
            setPanOffsetX(dragStartPanX_ - delta.x() / eff);
            setPanOffsetY(dragStartPanY_ - delta.y() / eff);
        }
    }
    event->accept();
}

void ProcessingViewportItem::mouseReleaseEvent(QMouseEvent *event) {
    dragging_ = false;
    event->accept();
}

void ProcessingViewportItem::onImageChanged() {
    int imageWidth = 0;
    int imageHeight = 0;
    if (controller_ && controller_->outImage()) {
        imageWidth = controller_->outImage()->width;
        imageHeight = controller_->outImage()->height;
    }

    if (imageWidth != lastImageWidth_ || imageHeight != lastImageHeight_) {
        lastImageWidth_ = imageWidth;
        lastImageHeight_ = imageHeight;
        if (zoomScale_ != 1.0) {
            zoomScale_ = 1.0;
            emit zoomScaleChanged();
        }
        if (panOffsetX_ != 0.0 || panOffsetY_ != 0.0) {
            panOffsetX_ = 0.0;
            panOffsetY_ = 0.0;
            emit panOffsetChanged();
        }
    }
    update();
}
