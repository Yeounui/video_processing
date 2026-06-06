#include "OpenCvImageBridge.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <limits>

namespace {

bool hasSupportedShape(const ImageBuffer& buffer)
{
    if (buffer.width < 0 || buffer.height < 0)
        return false;
    if (buffer.channels != 3 && buffer.channels != 4)
        return false;

    const auto width = static_cast<size_t>(buffer.width);
    const auto height = static_cast<size_t>(buffer.height);
    const auto channels = static_cast<size_t>(buffer.channels);
    const auto maxSize = std::numeric_limits<size_t>::max();

    if (width != 0 && height > maxSize / width)
        return false;
    const size_t pixels = width * height;
    if (pixels != 0 && channels > maxSize / pixels)
        return false;

    return buffer.data.size() == pixels * channels;
}

int cvTypeForChannels(int channels)
{
    return channels == 4 ? CV_8UC4 : CV_8UC3;
}

int channelsForMatType(const cv::Mat& mat)
{
    if (mat.depth() != CV_8U)
        return 0;
    const int channels = mat.channels();
    return (channels == 3 || channels == 4) ? channels : 0;
}

bool assignContinuousMat(const cv::Mat& mat, ImageBuffer& dst)
{
    const int channels = channelsForMatType(mat);
    if (channels == 0)
        return false;

    ImageBuffer next;
    next.width = mat.cols;
    next.height = mat.rows;
    next.channels = channels;
    next.data.resize(static_cast<size_t>(next.width) * next.height * next.channels);

    if (!next.data.empty()) {
        if (mat.isContinuous()) {
            const auto byteCount = next.data.size();
            std::copy(mat.data, mat.data + byteCount, next.data.begin());
        } else {
            const size_t rowBytes = static_cast<size_t>(next.width) * next.channels;
            for (int y = 0; y < next.height; ++y) {
                const uint8_t* row = mat.ptr<uint8_t>(y);
                std::copy(row, row + rowBytes, next.data.begin() + y * rowBytes);
            }
        }
    }

    dst = std::move(next);
    return true;
}

} // namespace

namespace OpenCvImageBridge {

bool isSupportedImageBuffer(const ImageBuffer& buffer)
{
    return hasSupportedShape(buffer);
}

cv::Mat mutableMatView(ImageBuffer& buffer)
{
    if (!hasSupportedShape(buffer))
        return {};
    return cv::Mat(buffer.height, buffer.width, cvTypeForChannels(buffer.channels),
                   buffer.data.data());
}

cv::Mat constMatView(const ImageBuffer& buffer)
{
    if (!hasSupportedShape(buffer))
        return {};
    return cv::Mat(buffer.height, buffer.width, cvTypeForChannels(buffer.channels),
                   const_cast<uint8_t*>(buffer.data.data()));
}

bool copyMatToImageBuffer(const cv::Mat& mat, ImageBuffer& dst, ColorOrder colorOrder)
{
    if (channelsForMatType(mat) == 0)
        return false;

    if (mat.empty()) {
        ImageBuffer next;
        next.width = mat.cols;
        next.height = mat.rows;
        next.channels = mat.channels();
        dst = std::move(next);
        return true;
    }

    if (colorOrder == ColorOrder::Rgb)
        return assignContinuousMat(mat, dst);

    cv::Mat converted;
    if (mat.channels() == 3)
        cv::cvtColor(mat, converted, cv::COLOR_BGR2RGB);
    else
        cv::cvtColor(mat, converted, cv::COLOR_BGRA2RGBA);
    return assignContinuousMat(converted, dst);
}

cv::Mat copyImageBufferToMat(const ImageBuffer& src, ColorOrder colorOrder)
{
    const cv::Mat view = constMatView(src);
    if (view.empty() && !hasSupportedShape(src))
        return {};
    if (view.empty())
        return view.clone();

    if (colorOrder == ColorOrder::Rgb)
        return view.clone();

    cv::Mat converted;
    if (src.channels == 3)
        cv::cvtColor(view, converted, cv::COLOR_RGB2BGR);
    else
        cv::cvtColor(view, converted, cv::COLOR_RGBA2BGRA);
    return converted;
}

} // namespace OpenCvImageBridge
