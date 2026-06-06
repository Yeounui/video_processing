#include "ImageIoService.h"
#include "OpenCvImageBridge.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <QString>
#include <string>

namespace {

std::string toOpenCvPath(const QString& path)
{
    return path.toUtf8().toStdString();
}

bool pathUsesJpegExtension(const QString& path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"));
}

} // namespace

std::shared_ptr<ImageBuffer> ImageIoService::load(const QString &path)
{
    const cv::Mat decoded = cv::imread(toOpenCvPath(path), cv::IMREAD_UNCHANGED);
    if (decoded.empty() || decoded.depth() != CV_8U)
        return nullptr;

    cv::Mat boundary;
    OpenCvImageBridge::ColorOrder colorOrder = OpenCvImageBridge::ColorOrder::Bgr;
    switch (decoded.channels()) {
    case 1:
        cv::cvtColor(decoded, boundary, cv::COLOR_GRAY2RGB);
        colorOrder = OpenCvImageBridge::ColorOrder::Rgb;
        break;
    case 3:
    case 4:
        boundary = decoded;
        colorOrder = OpenCvImageBridge::ColorOrder::Bgr;
        break;
    default:
        return nullptr;
    }

    auto buf = std::make_shared<ImageBuffer>();
    if (!OpenCvImageBridge::copyMatToImageBuffer(boundary, *buf, colorOrder))
        return nullptr;
    return buf;
}

bool ImageIoService::save(const ImageBuffer &buf, const QString &path)
{
    if (!OpenCvImageBridge::isSupportedImageBuffer(buf) || buf.width <= 0 || buf.height <= 0)
        return false;

    cv::Mat encoded = OpenCvImageBridge::copyImageBufferToMat(buf, OpenCvImageBridge::ColorOrder::Bgr);
    if (encoded.empty())
        return false;

    if (pathUsesJpegExtension(path) && encoded.channels() == 4)
        cv::cvtColor(encoded, encoded, cv::COLOR_BGRA2BGR);

    try {
        return cv::imwrite(toOpenCvPath(path), encoded);
    } catch (const cv::Exception&) {
        return false;
    }
}
