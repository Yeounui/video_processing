#include "ProcessingViewportItem.h"
#include "ProcessingController.h"
#include "ImageBuffer.h"
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QImage>
#include <algorithm>

ProcessingViewportItem::ProcessingViewportItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

ProcessingController *ProcessingViewportItem::controller() const
{
    return controller_;
}

void ProcessingViewportItem::setController(ProcessingController *ctrl)
{
    if (controller_ == ctrl)
        return;

    // Disconnect old controller signals
    if (controller_) {
        disconnect(controller_, &ProcessingController::imageChanged, this, &ProcessingViewportItem::onImageChanged);
    }

    controller_ = ctrl;
    uploadedVersion_ = std::numeric_limits<quint64>::max();

    // Connect new controller signals
    if (controller_) {
        connect(controller_, &ProcessingController::imageChanged, this, &ProcessingViewportItem::onImageChanged);
    }

    emit controllerChanged();
    update();
}

void ProcessingViewportItem::onImageChanged()
{
    update();
}

void ProcessingViewportItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();  // Recalculate letterbox on resize
}

QSGNode *ProcessingViewportItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(old);

    // If no controller or no image, clean up and return null
    if (!controller_ || !controller_->outImage()) {
        delete node;
        return nullptr;
    }

    // updatePaintNode() is called on the render thread with GUI thread blocked (Qt sync phase).
    // Reads of controller_->outImage() and outImageVersion() are safe here without additional locking.
    // Snapshot outImage_ (shared_ptr copy is safe on sync phase per D34)
    auto snapshot = controller_->outImage();
    quint64 version = controller_->outImageVersion();

    // Create node if needed
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
    }

    // Upload texture only if version changed (D6: prevent redundant uploads)
    if (version != uploadedVersion_) {
        QImage img = QImage(snapshot->data.data(), snapshot->width, snapshot->height,
                            snapshot->width * 3, QImage::Format_RGB888).copy();
        node->setTexture(window()->createTextureFromImage(img));
        uploadedVersion_ = version;
    }

    // Letterbox calculation (D29: GL_NEAREST + simple letterbox)
    float iw = snapshot->width;
    float ih = snapshot->height;
    float vw = width();
    float vh = height();

    // Scale to fit within viewport while maintaining aspect ratio
    float scale = std::min(vw / iw, vh / ih);

    // Center the scaled image
    float cx = (vw - iw * scale) * 0.5f;
    float cy = (vh - ih * scale) * 0.5f;

    node->setRect(cx, cy, iw * scale, ih * scale);
    node->setFiltering(QSGTexture::Nearest);

    return node;
}
