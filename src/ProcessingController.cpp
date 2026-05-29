#include "ProcessingController.h"
#include "ImageIoService.h"
#include <QFileInfo>

ProcessingController::ProcessingController(QObject *parent)
    : QObject(parent)
{
}

bool ProcessingController::hasImage() const
{
    return inImage_ != nullptr;
}

bool ProcessingController::canSave() const
{
    return hasImage() && sourceType_ == SourceType::SOURCE_IMAGE;
}

QString ProcessingController::sourceFileName() const
{
    return sourceFileName_;
}

int ProcessingController::imageWidth() const
{
    return inImage_ ? inImage_->width : 0;
}

int ProcessingController::imageHeight() const
{
    return inImage_ ? inImage_->height : 0;
}

std::shared_ptr<ImageBuffer> ProcessingController::outImage() const
{
    return outImage_;
}

quint64 ProcessingController::outImageVersion() const
{
    return outImageVersion_;
}

void ProcessingController::openImage(const QUrl &url)
{
    QString path = url.toLocalFile();
    auto newBuf = ImageIoService::load(path);

    // Error preservation: if load fails, do NOT touch inImage_/outImage_
    if (!newBuf) {
        emit errorOccurred(QString("Failed to load image: %1").arg(path));
        return;
    }

    // Load succeeded, now update state
    inImage_ = newBuf;
    outImage_ = std::make_shared<ImageBuffer>(*newBuf);  // deep copy
    ++outImageVersion_;
    sourceType_ = SourceType::SOURCE_IMAGE;
    sourceFileName_ = QFileInfo(path).fileName();

    // Emit all relevant signals
    emit sourceChanged();
    emit hasImageChanged();
    emit canSaveChanged();
    emit imageChanged();
}

void ProcessingController::saveImage(const QUrl &url)
{
    if (!canSave()) {
        emit errorOccurred("Cannot save: no image loaded or source type is not SOURCE_IMAGE");
        return;
    }

    QString path = url.toLocalFile();
    if (!ImageIoService::save(*outImage_, path)) {
        emit errorOccurred(QString("Failed to save image: %1").arg(path));
    }
}

void ProcessingController::reset()
{
    if (!inImage_) {
        return;
    }

    // D21: Reset semantics: outImage_ restored from inImage_ (deep copy only, do NOT re-read disk)
    outImage_ = std::make_shared<ImageBuffer>(*inImage_);
    ++outImageVersion_;
    emit imageChanged();
}
