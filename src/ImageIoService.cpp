#include "ImageIoService.h"
#include <QImage>
#include <algorithm>

std::shared_ptr<ImageBuffer> ImageIoService::load(const QString &path)
{
    QImage img(path);
    if (img.isNull())
        return nullptr;

    const bool hasAlpha = img.hasAlphaChannel();
    img = img.convertToFormat(hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    if (img.isNull())
        return nullptr;

    auto buf = std::make_shared<ImageBuffer>();
    buf->width = img.width();
    buf->height = img.height();
    buf->channels = hasAlpha ? 4 : 3;
    buf->data.resize(buf->width * buf->height * buf->channels);

    for (int y = 0; y < buf->height; ++y) {
        const uint8_t *scanline = img.constScanLine(y);
        std::copy(scanline, scanline + buf->width * buf->channels,
                  buf->data.begin() + y * buf->width * buf->channels);
    }

    return buf;
}

bool ImageIoService::save(const ImageBuffer &buf, const QString &path)
{
    const QImage::Format format = buf.channels == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    QImage img(buf.data.data(), buf.width, buf.height, buf.width * buf.channels, format);
    return img.save(path);
}
