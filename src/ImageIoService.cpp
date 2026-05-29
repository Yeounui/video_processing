#include "ImageIoService.h"
#include <QImage>

std::shared_ptr<ImageBuffer> ImageIoService::load(const QString &path)
{
    QImage img(path);
    if (img.isNull())
        return nullptr;

    img = img.convertToFormat(QImage::Format_RGB888);
    if (img.isNull())
        return nullptr;

    auto buf = std::make_shared<ImageBuffer>();
    buf->width = img.width();
    buf->height = img.height();
    buf->channels = 3;
    buf->data.resize(buf->width * buf->height * buf->channels);

    // Copy scanlines into data buffer (stride = width * 3)
    for (int y = 0; y < buf->height; ++y) {
        const uint8_t *scanline = img.constScanLine(y);
        std::copy(scanline, scanline + buf->width * 3,
                  buf->data.begin() + y * buf->width * 3);
    }

    return buf;
}

bool ImageIoService::save(const ImageBuffer &buf, const QString &path)
{
    QImage img(buf.data.data(), buf.width, buf.height, buf.width * 3, QImage::Format_RGB888);
    return img.save(path);
}
