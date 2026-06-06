#pragma once

#include "ImageBuffer.h"

#include <opencv2/core.hpp>

namespace OpenCvImageBridge {

enum class ColorOrder {
    Rgb,
    Bgr
};

bool isSupportedImageBuffer(const ImageBuffer& buffer);

cv::Mat mutableMatView(ImageBuffer& buffer);

// OpenCV Mat does not carry const storage, so callers must treat this view as
// read-only and must not pass it to APIs that write in place.
cv::Mat constMatView(const ImageBuffer& buffer);

bool copyMatToImageBuffer(const cv::Mat& mat,
                          ImageBuffer& dst,
                          ColorOrder colorOrder = ColorOrder::Rgb);

cv::Mat copyImageBufferToMat(const ImageBuffer& src,
                             ColorOrder colorOrder = ColorOrder::Rgb);

} // namespace OpenCvImageBridge
