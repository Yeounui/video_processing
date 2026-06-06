#include "GpuEffectPipeline.h"
#include <QDebug>
#include <cmath>

namespace {

class ScopedFramebufferViewportRestore {
public:
    explicit ScopedFramebufferViewportRestore(QOpenGLFunctions_3_3_Core &gl)
        : gl_(gl)
    {
        gl_.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer_);
        gl_.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer_);
        gl_.glGetIntegerv(GL_VIEWPORT, viewport_);
    }

    ~ScopedFramebufferViewportRestore()
    {
        gl_.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer_));
        gl_.glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer_));
        gl_.glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    }

private:
    QOpenGLFunctions_3_3_Core &gl_;
    GLint drawFramebuffer_ = 0;
    GLint readFramebuffer_ = 0;
    GLint viewport_[4] = {0, 0, 0, 0};
};

} // namespace

// ============================================================================
// Static Shader Sources
// ============================================================================

// Shared vertex shader for all effects (fullscreen quad)
static const char *vertShaderSrc = R"glsl(
#version 330 core
in vec2 a_pos;
out vec2 v_uv;
void main() {
    v_uv = a_pos * 0.5 + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)glsl";

// ============================================================================
// Fragment Shader Sources (per algorithm)
// ============================================================================

const char *GpuEffectPipeline::fragmentSource(int algorithmId) {
    switch (algorithmId) {
    case 1: {  // Brightness
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform float u_amount;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    rgb = clamp(rgb + vec3(u_amount / 255.0), 0.0, 1.0);
    fragColor = vec4(rgb, texel.a);
}
)glsl";
        return src;
    }
    case 2: {  // Multiply
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform float u_factor;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    rgb = clamp(rgb * u_factor, 0.0, 1.0);
    fragColor = vec4(rgb, texel.a);
}
)glsl";
        return src;
    }
    case 3: {  // Gamma
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform float u_gamma;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    rgb = pow(clamp(rgb, 1e-6, 1.0), vec3(u_gamma));
    fragColor = vec4(rgb, texel.a);
}
)glsl";
        return src;
    }
    case 4: {  // Fixed Threshold
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform float u_threshold;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    float v = luma >= u_threshold ? 1.0 : 0.0;
    fragColor = vec4(vec3(v), texel.a);
}
)glsl";
        return src;
    }
    case 6: {  // Bitwise AND
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform uint u_mask;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    uvec3 ci = uvec3(round(texel.rgb * 255.0));
    ci = ci & uvec3(u_mask);
    fragColor = vec4(vec3(ci) / 255.0, texel.a);
}
)glsl";
        return src;
    }
    case 7: {  // Flip
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform int u_flipH;
uniform int u_flipV;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 uv = v_uv;
    if (u_flipH > 0) uv.x = 1.0 - uv.x;
    if (u_flipV > 0) uv.y = 1.0 - uv.y;
    fragColor = texture(u_tex, uv);
}
)glsl";
        return src;
    }
    case 9: {  // Emboss (3x3 kernel)
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 s = u_invSize;
    float alpha = texture(u_tex, v_uv).a;
    vec3 color = vec3(0.5);
    color += -2.0*texture(u_tex, v_uv+vec2(-s.x,-s.y)).rgb;
    color += -1.0*texture(u_tex, v_uv+vec2( 0.0,-s.y)).rgb;
    color += -1.0*texture(u_tex, v_uv+vec2(-s.x, 0.0)).rgb;
    color +=  1.0*texture(u_tex, v_uv+vec2( s.x, 0.0)).rgb;
    color +=  1.0*texture(u_tex, v_uv+vec2( 0.0, s.y)).rgb;
    color +=  2.0*texture(u_tex, v_uv+vec2( s.x, s.y)).rgb;
    fragColor = vec4(clamp(color, 0.0, 1.0), alpha);
}
)glsl";
        return src;
    }
    case 11: {  // 3x3 Blur
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    float alpha = texture(u_tex, v_uv).a;
    vec3 color = vec3(0.0);
    for (int dy=-1; dy<=1; dy++)
        for (int dx=-1; dx<=1; dx++)
            color += texture(u_tex, v_uv + vec2(float(dx), float(dy))*u_invSize).rgb;
    fragColor = vec4(color/9.0, alpha);
}
)glsl";
        return src;
    }
    case 12: {  // 5x5 Blur
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    float alpha = texture(u_tex, v_uv).a;
    vec3 color = vec3(0.0);
    for (int dy=-2; dy<=2; dy++)
        for (int dx=-2; dx<=2; dx++)
            color += texture(u_tex, v_uv + vec2(float(dx), float(dy))*u_invSize).rgb;
    fragColor = vec4(color/25.0, alpha);
}
)glsl";
        return src;
    }
    case 14: {  // Sharpen
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
uniform float u_amount;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    float a = u_amount;
    vec4 texel = texture(u_tex, v_uv);
    vec3 color = (1.0+4.0*a)*texel.rgb
        -a*texture(u_tex, v_uv+vec2(0.0, u_invSize.y)).rgb
        -a*texture(u_tex, v_uv-vec2(0.0, u_invSize.y)).rgb
        -a*texture(u_tex, v_uv+vec2(u_invSize.x, 0.0)).rgb
        -a*texture(u_tex, v_uv-vec2(u_invSize.x, 0.0)).rgb;
    fragColor = vec4(clamp(color, 0.0, 1.0), texel.a);
}
)glsl";
        return src;
    }
    case 19: {  // Horizontal Edge (Sobel X)
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 s = u_invSize;
    float alpha = texture(u_tex, v_uv).a;
    vec3 gx = -1.0*texture(u_tex,v_uv+vec2(-s.x,-s.y)).rgb
        -2.0*texture(u_tex,v_uv+vec2(-s.x, 0.0)).rgb
        -1.0*texture(u_tex,v_uv+vec2(-s.x, s.y)).rgb
        +1.0*texture(u_tex,v_uv+vec2( s.x,-s.y)).rgb
        +2.0*texture(u_tex,v_uv+vec2( s.x, 0.0)).rgb
        +1.0*texture(u_tex,v_uv+vec2( s.x, s.y)).rgb;
    fragColor = vec4(clamp(abs(gx)/4.0, 0.0, 1.0), alpha);
}
)glsl";
        return src;
    }
    case 20: {  // Vertical Edge (Sobel Y)
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 s = u_invSize;
    float alpha = texture(u_tex, v_uv).a;
    vec3 gy = -1.0*texture(u_tex,v_uv+vec2(-s.x,-s.y)).rgb
        -2.0*texture(u_tex,v_uv+vec2( 0.0,-s.y)).rgb
        -1.0*texture(u_tex,v_uv+vec2( s.x,-s.y)).rgb
        +1.0*texture(u_tex,v_uv+vec2(-s.x, s.y)).rgb
        +2.0*texture(u_tex,v_uv+vec2( 0.0, s.y)).rgb
        +1.0*texture(u_tex,v_uv+vec2( s.x, s.y)).rgb;
    fragColor = vec4(clamp(abs(gy)/4.0, 0.0, 1.0), alpha);
}
)glsl";
        return src;
    }
    case 21: {  // Laplacian
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 s = u_invSize;
    vec4 texel = texture(u_tex, v_uv);
    vec3 c = texel.rgb;
    vec3 lap = 4.0*c
        - texture(u_tex,v_uv+vec2(0.0,s.y)).rgb
        - texture(u_tex,v_uv-vec2(0.0,s.y)).rgb
        - texture(u_tex,v_uv+vec2(s.x,0.0)).rgb
        - texture(u_tex,v_uv-vec2(s.x,0.0)).rgb;
    fragColor = vec4(clamp(lap + 0.5, 0.0, 1.0), texel.a);
}
)glsl";
        return src;
    }
    case 26: {  // Grayscale Average
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    float g = (rgb.r + rgb.g + rgb.b) / 3.0;
    fragColor = vec4(vec3(g), texel.a);
}
)glsl";
        return src;
    }
    case 27: {  // Grayscale Luminosity
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    float g = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    fragColor = vec4(vec3(g), texel.a);
}
)glsl";
        return src;
    }
    case 28: {  // Grayscale Lightness
        static const char *src = R"glsl(
#version 330 core
uniform sampler2D u_tex;
uniform vec2 u_invSize;
in vec2 v_uv;
out vec4 fragColor;
void main() {
    vec4 texel = texture(u_tex, v_uv);
    vec3 rgb = texel.rgb;
    float g = (max(rgb.r, max(rgb.g, rgb.b)) + min(rgb.r, min(rgb.g, rgb.b))) * 0.5;
    fragColor = vec4(vec3(g), texel.a);
}
)glsl";
        return src;
    }
    default:
        return nullptr;
    }
}

// ============================================================================
// Public Interface
// ============================================================================

bool GpuEffectPipeline::supportsAlgorithm(int algorithmId) {
    static const int supported[] = {1, 2, 3, 4, 6, 7, 9, 11, 12, 14, 19, 20, 21, 26, 27, 28};
    for (int s : supported) {
        if (s == algorithmId) return true;
    }
    return false;
}

bool GpuEffectPipeline::initialize() {
    if (initialized_) return true;

    if (!gl_.initializeOpenGLFunctions()) {
        qWarning() << "GpuEffectPipeline: Failed to initialize OpenGL 3.3 Core functions";
        return false;
    }

    // Create VAO and VBO for fullscreen quad (triangle strip)
    float verts[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};
    gl_.glGenVertexArrays(1, &vao_);
    gl_.glGenBuffers(1, &vbo_);
    gl_.glBindVertexArray(vao_);
    gl_.glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_.glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    // Vertex attribute for position (location 0)
    gl_.glEnableVertexAttribArray(0);
    gl_.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    gl_.glBindVertexArray(0);

    initialized_ = true;
    return true;
}

void GpuEffectPipeline::destroy() {
    if (!initialized_) return;

    for (auto &[id, prog] : programs_) {
        if (prog.id) gl_.glDeleteProgram(prog.id);
    }
    programs_.clear();

    if (fbo_) gl_.glDeleteFramebuffers(1, &fbo_);
    if (srcTex_) gl_.glDeleteTextures(1, &srcTex_);
    if (dstTex_) gl_.glDeleteTextures(1, &dstTex_);
    if (vbo_) gl_.glDeleteBuffers(1, &vbo_);
    if (vao_) gl_.glDeleteVertexArrays(1, &vao_);

    fbo_ = srcTex_ = dstTex_ = vbo_ = vao_ = 0;
    cachedW_ = cachedH_ = 0;
    initialized_ = false;
}

std::shared_ptr<ImageBuffer> GpuEffectPipeline::applyStaticEffect(
    const ImageBuffer &src, int algorithmId, const QVariantMap &params)
{
    if (!initialize()) {
        qWarning() << "GpuEffectPipeline: Failed to initialize";
        return nullptr;
    }

    ScopedFramebufferViewportRestore restore(gl_);

    if (!ensureTextures(src.width, src.height)) {
        qWarning() << "GpuEffectPipeline: Failed to ensure textures";
        return nullptr;
    }

    uploadSrc(src);

    GLuint prog = getOrCompileProgram(algorithmId);
    if (!prog) {
        qWarning() << "GpuEffectPipeline: Failed to get/compile shader for algorithm" << algorithmId;
        return nullptr;
    }

    // Bind FBO and render
    gl_.glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl_.glViewport(0, 0, src.width, src.height);
    gl_.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl_.glClear(GL_COLOR_BUFFER_BIT);

    gl_.glUseProgram(prog);
    setUniforms(prog, algorithmId, params, src.width, src.height);

    gl_.glBindVertexArray(vao_);
    gl_.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_.glBindVertexArray(0);

    gl_.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return readback(src.width, src.height, src.channels);
}

// ============================================================================
// Private Implementation
// ============================================================================

bool GpuEffectPipeline::ensureTextures(int w, int h) {
    if (w != cachedW_ || h != cachedH_) {
        // Delete old textures and FBO
        if (fbo_) gl_.glDeleteFramebuffers(1, &fbo_);
        if (srcTex_) gl_.glDeleteTextures(1, &srcTex_);
        if (dstTex_) gl_.glDeleteTextures(1, &dstTex_);

        fbo_ = srcTex_ = dstTex_ = 0;

        // Create source texture (GL_RGBA8)
        gl_.glGenTextures(1, &srcTex_);
        gl_.glBindTexture(GL_TEXTURE_2D, srcTex_);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // Create destination texture (GL_RGBA8)
        gl_.glGenTextures(1, &dstTex_);
        gl_.glBindTexture(GL_TEXTURE_2D, dstTex_);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // Create FBO and attach destination texture
        gl_.glGenFramebuffers(1, &fbo_);
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        gl_.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTex_, 0);

        GLenum status = gl_.glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            qWarning() << "GpuEffectPipeline: Framebuffer incomplete, status =" << status;
            gl_.glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        gl_.glBindFramebuffer(GL_FRAMEBUFFER, 0);

        cachedW_ = w;
        cachedH_ = h;
    }

    return true;
}

void GpuEffectPipeline::uploadSrc(const ImageBuffer &src) {
    gl_.glBindTexture(GL_TEXTURE_2D, srcTex_);
    gl_.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (src.channels == 4) {
        gl_.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src.width, src.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, src.data.data());
    } else {
        std::vector<uint8_t> rgbaData(src.width * src.height * 4);
        for (int i = 0; i < src.width * src.height; ++i) {
            rgbaData[i * 4 + 0] = src.data[i * 3 + 0];
            rgbaData[i * 4 + 1] = src.data[i * 3 + 1];
            rgbaData[i * 4 + 2] = src.data[i * 3 + 2];
            rgbaData[i * 4 + 3] = 255;
        }
        gl_.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src.width, src.height,
                            GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());
    }
}

std::shared_ptr<ImageBuffer> GpuEffectPipeline::readback(int w, int h, int channels) {
    gl_.glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl_.glPixelStorei(GL_PACK_ALIGNMENT, 1);

    auto result = std::make_shared<ImageBuffer>();
    result->width = w;
    result->height = h;
    result->channels = channels == 4 ? 4 : 3;
    result->data.resize(w * h * result->channels);

    gl_.glReadPixels(0, 0, w, h, result->channels == 4 ? GL_RGBA : GL_RGB,
                     GL_UNSIGNED_BYTE, result->data.data());

    gl_.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return result;
}

GLuint GpuEffectPipeline::getOrCompileProgram(int algorithmId) {
    auto it = programs_.find(algorithmId);
    if (it != programs_.end()) {
        return it->second.failed ? 0 : it->second.id;
    }

    const char *fragSrc = fragmentSource(algorithmId);
    if (!fragSrc) {
        programs_[algorithmId].failed = true;
        return 0;
    }

    auto compileShader = [&](GLenum type, const char *src) -> GLuint {
        GLuint s = gl_.glCreateShader(type);
        gl_.glShaderSource(s, 1, &src, nullptr);
        gl_.glCompileShader(s);
        GLint ok = 0;
        gl_.glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            gl_.glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            QByteArray log(len, '\0');
            gl_.glGetShaderInfoLog(s, len, nullptr, log.data());
            qWarning() << "GpuEffectPipeline: shader compile failed (alg" << algorithmId
                       << ", type" << type << "):" << log;
            gl_.glDeleteShader(s);
            return 0;
        }
        return s;
    };

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertShaderSrc);
    if (!vs) { programs_[algorithmId].failed = true; return 0; }

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fs) { gl_.glDeleteShader(vs); programs_[algorithmId].failed = true; return 0; }

    GLuint prog = gl_.glCreateProgram();
    gl_.glAttachShader(prog, vs);
    gl_.glAttachShader(prog, fs);
    gl_.glLinkProgram(prog);
    gl_.glDeleteShader(vs);
    gl_.glDeleteShader(fs);

    GLint linked = 0;
    gl_.glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        gl_.glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        QByteArray log(len, '\0');
        gl_.glGetProgramInfoLog(prog, len, nullptr, log.data());
        qWarning() << "GpuEffectPipeline: shader link failed (alg" << algorithmId << "):" << log;
        gl_.glDeleteProgram(prog);
        programs_[algorithmId].failed = true;
        return 0;
    }

    programs_[algorithmId] = Program{prog, false};
    return prog;
}

void GpuEffectPipeline::setUniforms(GLuint prog, int algorithmId, const QVariantMap &params, int w, int h) {
    gl_.glUseProgram(prog);

    // Always set u_tex (location 0) and u_invSize
    GLint texLoc = gl_.glGetUniformLocation(prog, "u_tex");
    if (texLoc >= 0) {
        gl_.glUniform1i(texLoc, 0);
    }
    gl_.glActiveTexture(GL_TEXTURE0);
    gl_.glBindTexture(GL_TEXTURE_2D, srcTex_);

    GLint invSizeLoc = gl_.glGetUniformLocation(prog, "u_invSize");
    if (invSizeLoc >= 0) {
        float invW = 1.0f / w;
        float invH = 1.0f / h;
        gl_.glUniform2f(invSizeLoc, invW, invH);
    }

    // Algorithm-specific uniforms
    switch (algorithmId) {
    case 1: {  // Brightness
        GLint amountLoc = gl_.glGetUniformLocation(prog, "u_amount");
        if (amountLoc >= 0) {
            float amount = params.value("amount", 0.0f).toFloat();
            gl_.glUniform1f(amountLoc, amount);
        }
        break;
    }
    case 2: {  // Multiply
        GLint factorLoc = gl_.glGetUniformLocation(prog, "u_factor");
        if (factorLoc >= 0) {
            float factor = params.value("factor", 1.0f).toFloat();
            gl_.glUniform1f(factorLoc, factor);
        }
        break;
    }
    case 3: {  // Gamma
        GLint gammaLoc = gl_.glGetUniformLocation(prog, "u_gamma");
        if (gammaLoc >= 0) {
            float gamma = params.value("gamma", 1.0f).toFloat();
            gl_.glUniform1f(gammaLoc, gamma);
        }
        break;
    }
    case 4: {  // Fixed Threshold
        GLint thresholdLoc = gl_.glGetUniformLocation(prog, "u_threshold");
        if (thresholdLoc >= 0) {
            int thresholdInt = params.value("threshold", 128).toInt();
            float threshold = thresholdInt / 255.0f;
            gl_.glUniform1f(thresholdLoc, threshold);
        }
        break;
    }
    case 6: {  // Bitwise AND
        GLint maskLoc = gl_.glGetUniformLocation(prog, "u_mask");
        if (maskLoc >= 0) {
            uint mask = params.value("mask", 255u).toUInt();
            gl_.glUniform1ui(maskLoc, mask);
        }
        break;
    }
    case 7: {  // Flip
        GLint flipHLoc = gl_.glGetUniformLocation(prog, "u_flipH");
        GLint flipVLoc = gl_.glGetUniformLocation(prog, "u_flipV");
        const QString mode = params.value("mode", QStringLiteral("H")).toString();
        if (flipHLoc >= 0) {
            int flipH = (mode == QStringLiteral("H") || mode == QStringLiteral("Both")
                         || params.value("horizontal", false).toBool()) ? 1 : 0;
            gl_.glUniform1i(flipHLoc, flipH);
        }
        if (flipVLoc >= 0) {
            int flipV = (mode == QStringLiteral("V") || mode == QStringLiteral("Both")
                         || params.value("vertical", false).toBool()) ? 1 : 0;
            gl_.glUniform1i(flipVLoc, flipV);
        }
        break;
    }
    case 14: {  // Sharpen
        GLint amountLoc = gl_.glGetUniformLocation(prog, "u_amount");
        if (amountLoc >= 0) {
            float amount = params.value("amount", 0.5f).toFloat();
            gl_.glUniform1f(amountLoc, amount);
        }
        break;
    }
    default:
        break;
    }
}
