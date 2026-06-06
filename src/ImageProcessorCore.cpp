#include "ImageProcessorCore.h"
#include "OpenCvImageBridge.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

// Luminance formula: Y = 0.299*R + 0.587*G + 0.114*B
inline double lum(uint8_t r, uint8_t g, uint8_t b) {
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

// Clamp double to [0,255], round, return uint8_t
inline uint8_t clamp8(double v) {
    long rounded = std::lround(v);
    return static_cast<uint8_t>(std::max(0L, std::min(255L, rounded)));
}

// Clamp integer index to [0, max-1]
inline int clampIdx(int v, int maxExcl) {
    return std::max(0, std::min(maxExcl - 1, v));
}

// Read pixel channel c at (x,y), clamped to edge
inline uint8_t px(const ImageBuffer& img, int x, int y, int c) {
    x = clampIdx(x, img.width);
    y = clampIdx(y, img.height);
    return img.data[(y * img.width + x) * img.channels + c];
}

// Write pixel channel c at (x,y) in dst (already sized correctly)
inline void setPx(ImageBuffer& img, int x, int y, int c, uint8_t v) {
    img.data[(y * img.width + x) * img.channels + c] = v;
}

// Resize dst to match src, zero-fill RGB, and preserve alpha when present.
inline void resizeDst(const ImageBuffer& src, ImageBuffer& dst) {
    dst.width = src.width;
    dst.height = src.height;
    dst.channels = src.channels;
    dst.data.assign(src.width * src.height * src.channels, 0);
    if (src.channels >= 4) {
        for (int y = 0; y < src.height; ++y)
            for (int x = 0; x < src.width; ++x)
                setPx(dst, x, y, 3, px(src, x, y, 3));
    }
}

inline void clearTransparentRgb(ImageBuffer& img) {
    if (img.channels < 4)
        return;

    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x)
            if (px(img, x, y, 3) == 0) {
                setPx(img, x, y, 0, 0);
                setPx(img, x, y, 1, 0);
                setPx(img, x, y, 2, 0);
    }
}

static cv::Mat rgbChannelsMat(const ImageBuffer& src) {
    const cv::Mat srcView = OpenCvImageBridge::constMatView(src);
    if (srcView.empty())
        return {};
    if (src.channels == 3)
        return srcView;

    std::vector<cv::Mat> channels;
    cv::split(srcView, channels);
    cv::Mat rgb;
    cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, rgb);
    return rgb;
}

static bool writeRgbChannels(const cv::Mat& rgb, ImageBuffer& dst) {
    if (rgb.empty())
        return true;

    cv::Mat dstView = OpenCvImageBridge::mutableMatView(dst);
    if (dstView.empty())
        return false;

    if (dst.channels == 3) {
        rgb.copyTo(dstView);
        return true;
    }

    std::vector<cv::Mat> dstChannels;
    std::vector<cv::Mat> rgbChannels;
    cv::split(dstView, dstChannels);
    cv::split(rgb, rgbChannels);
    dstChannels[0] = rgbChannels[0];
    dstChannels[1] = rgbChannels[1];
    dstChannels[2] = rgbChannels[2];
    cv::merge(dstChannels, dstView);
    return true;
}

static cv::Mat luminanceMat(const cv::Mat& rgb) {
    cv::Mat rgb64;
    rgb.convertTo(rgb64, CV_64F);

    cv::Mat luminance;
    cv::transform(rgb64, luminance,
                  cv::Matx<double, 1, 3>(0.299, 0.587, 0.114));
    return luminance;
}

static cv::Mat mergeGrayToRgb(const cv::Mat& gray) {
    cv::Mat rgb;
    cv::merge(std::vector<cv::Mat>{gray, gray, gray}, rgb);
    return rgb;
}

static cv::Mat filterRgb(const cv::Mat& rgb, const cv::Mat& kernel, double delta = 0.0) {
    cv::Mat filtered64;
    cv::filter2D(rgb, filtered64, CV_64F, kernel, cv::Point(-1, -1), delta,
                 cv::BORDER_REPLICATE);

    cv::Mat filtered;
    filtered64.convertTo(filtered, CV_8U);
    return filtered;
}

static cv::Mat gaussianBlurRgb(const cv::Mat& rgb, int kernelSize, double sigma) {
    cv::Mat blurred;
    cv::GaussianBlur(rgb, blurred, cv::Size(kernelSize, kernelSize), sigma, sigma,
                     cv::BORDER_REPLICATE);
    return blurred;
}

static cv::Mat stretchRgbWithStats(const cv::Mat& rgb, int statMin, int statMax,
                                   bool multiplyBeforeDivide) {
    if (statMin == statMax)
        return rgb.clone();

    const int denominator = statMax - statMin;
    const double scale = 255.0 / (statMax - statMin);
    cv::Mat lut(1, 256, CV_8UC1);
    for (int i = 0; i < 256; ++i) {
        const double stretched = multiplyBeforeDivide
            ? ((i - statMin) * 255.0) / denominator
            : (i - statMin) * scale;
        lut.at<uint8_t>(0, i) = clamp8(stretched);
    }

    cv::Mat stretched;
    cv::LUT(rgb, lut, stretched);
    return stretched;
}

static cv::Mat rgbaGeometryMat(const ImageBuffer& src) {
    const cv::Mat srcView = OpenCvImageBridge::constMatView(src);
    if (srcView.empty())
        return {};
    if (src.channels == 4)
        return srcView;

    cv::Mat rgba;
    cv::cvtColor(srcView, rgba, cv::COLOR_RGB2RGBA);
    return rgba;
}

// Build the 28-algorithm spec table
const std::vector<AlgorithmSpec>& ImageProcessorCore::specs() {
    static const std::vector<AlgorithmSpec> specs_table = {
        // 1. Brightness
        {1, "Brightness", "Point", {
            {QString("delta"), QString("Delta"), ParamType::Int, ControlType::Slider, -255, 255, 1, 30}
        }},
        // 2. Multiply
        {2, "Multiply", "Point", {
            {QString("factor"), QString("Factor"), ParamType::Float, ControlType::Slider, 0.0, 4.0, 0.01, 1.2}
        }},
        // 3. Gamma
        {3, "Gamma", "Point", {
            {QString("gamma"), QString("Gamma"), ParamType::Float, ControlType::Slider, 0.1, 5.0, 0.01, 2.2}
        }},
        // 4. Fixed Threshold
        {4, "Fixed Threshold", "Point", {
            {QString("threshold"), QString("Threshold"), ParamType::Int, ControlType::Slider, 0, 255, 1, 127}
        }},
        // 5. Average Threshold
        {5, "Average Threshold", "Point", {
            {QString("average"), QString("Average"), ParamType::Auto, ControlType::Slider, 0, 255, 1, 128, QStringList(), true}
        }},
        // 6. Bitwise AND
        {6, "Bitwise AND", "Point", {
            {QString("mask"), QString("Mask"), ParamType::Int, ControlType::SpinBox, 0, 255, 1, 0xC9}
        }},
        // 7. Flip
        {7, "Flip", "Geometry", {
            {QString("mode"), QString("Mode"), ParamType::Enum, ControlType::ComboBox, 0, 3, 1, QString("H"), QStringList() << "H" << "V" << "Both"}
        }},
        // 8. Rotate
        {8, "Rotate", "Geometry", {
            {QString("degree"), QString("Degree"), ParamType::Float, ControlType::Slider, -360, 360, 0.5, 30.0}
        }},
        // 9. Emboss
        {9, "Emboss", "Filter", {}},
        // 10. Contrast Stretch
        {10, "Contrast Stretch", "Filter", {}},
        // 11. 3x3 Blur
        {11, "3x3 Blur", "Filter", {}},
        // 12. 5x5 Blur
        {12, "5x5 Blur", "Filter", {}},
        // 13. Gaussian Blur
        {13, "Gaussian Blur", "Filter", {
            {QString("kernel"), QString("Kernel"), ParamType::Enum, ControlType::ComboBox, 0, 3, 1, QString("5"), QStringList() << "3" << "5" << "7"},
            {QString("sigma"), QString("Sigma"), ParamType::Float, ControlType::Slider, 0.1, 5.0, 0.1, 1.0}
        }},
        // 14. Sharpen
        {14, "Sharpen", "Filter", {
            {QString("alpha"), QString("Alpha"), ParamType::Float, ControlType::Slider, 0.0, 3.0, 0.05, 1.0}
        }},
        // 15. High-Pass Sharpen
        {15, "High-Pass Sharpen", "Filter", {
            {QString("strength"), QString("Strength"), ParamType::Float, ControlType::Slider, 0.0, 3.0, 0.05, 1.0}
        }},
        // 16. High-Boost
        {16, "High-Boost", "Filter", {
            {QString("beta"), QString("Beta"), ParamType::Float, ControlType::Slider, 0.0, 3.0, 0.05, 1.0}
        }},
        // 17. Diagonal Motion Blur
        {17, "Diagonal Motion Blur", "Filter", {
            {QString("distance"), QString("Distance"), ParamType::Int, ControlType::Slider, 3, 31, 2, 9}
        }},
        // 18. Horizontal Motion Blur
        {18, "Horizontal Motion Blur", "Filter", {
            {QString("distance"), QString("Distance"), ParamType::Int, ControlType::Slider, 3, 31, 2, 9}
        }},
        // 19. Horizontal Edge
        {19, "Horizontal Edge", "Edge", {
            {QString("scale"), QString("Scale"), ParamType::Float, ControlType::Slider, 0.1, 4.0, 0.1, 1.0}
        }},
        // 20. Vertical Edge
        {20, "Vertical Edge", "Edge", {
            {QString("scale"), QString("Scale"), ParamType::Float, ControlType::Slider, 0.1, 4.0, 0.1, 1.0}
        }},
        // 21. Laplacian
        {21, "Laplacian", "Edge", {}},
        // 22. DoG
        {22, "DoG", "Edge", {
            {QString("sigma1"), QString("Sigma 1"), ParamType::Float, ControlType::Slider, 0.1, 4.0, 0.1, 1.0},
            {QString("sigma2"), QString("Sigma 2"), ParamType::Float, ControlType::Slider, 0.1, 4.0, 0.1, 2.0},
            {QString("gain"), QString("Gain"), ParamType::Float, ControlType::Slider, 0.1, 4.0, 0.1, 1.0}
        }},
        // 23. Histogram Stretch
        {23, "Histogram Stretch", "Edge", {}},
        // 24. Endpoint Detection
        {24, "Endpoint Detection", "Edge", {
            {QString("threshold"), QString("Threshold"), ParamType::Int, ControlType::Slider, 0, 255, 1, 127}
        }},
        // 25. Median Smoothing
        {25, "Median Smoothing", "Morphology", {}},
        // 26. Grayscale Average
        {26, "Grayscale Average", "Grayscale", {}},
        // 27. Grayscale Luminosity
        {27, "Grayscale Luminosity", "Grayscale", {}},
        // 28. Grayscale Lightness
        {28, "Grayscale Lightness", "Grayscale", {}}
    };
    return specs_table;
}

// Apply algorithm by id
bool ImageProcessorCore::apply(const ImageBuffer& src, ImageBuffer& dst,
                               int algorithmId, const EffectParams& params) {
    if (algorithmId < 1 || algorithmId > 28) return false;

    resizeDst(src, dst);
    int W = src.width, H = src.height;

    switch (algorithmId) {
    case 1: { // Brightness
        int delta = params.value("delta", 30).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat result;
        cv::add(rgb, cv::Scalar(delta, delta, delta), result);
        writeRgbChannels(result, dst);
        break;
    }
    case 2: { // Multiply
        double factor = params.value("factor", 1.2).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat result;
        rgb.convertTo(result, CV_8U, factor);
        writeRgbChannels(result, dst);
        break;
    }
    case 3: { // Gamma
        double gamma = params.value("gamma", 2.2).toDouble();
        cv::Mat lut(1, 256, CV_8UC1);
        for (int i = 0; i < 256; ++i) {
            double normalized = i / 255.0;
            double corrected = std::pow(normalized, 1.0 / gamma) * 255.0;
            lut.at<uint8_t>(0, i) = clamp8(corrected);
        }

        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat result;
        cv::LUT(rgb, lut, result);
        writeRgbChannels(result, dst);
        break;
    }
    case 4: { // Fixed Threshold
        int threshold = params.value("threshold", 127).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat mask;
        cv::compare(luminanceMat(rgb), cv::Scalar(threshold), mask, cv::CMP_GE);
        writeRgbChannels(mergeGrayToRgb(mask), dst);
        break;
    }
    case 5: { // Average Threshold
        double average = params.value("stat_average", 128).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat mask;
        cv::compare(luminanceMat(rgb), cv::Scalar(average), mask, cv::CMP_GE);
        writeRgbChannels(mergeGrayToRgb(mask), dst);
        break;
    }
    case 6: { // Bitwise AND
        int mask = params.value("mask", 0xC9).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat result;
        cv::bitwise_and(rgb, cv::Scalar(mask, mask, mask), result);
        writeRgbChannels(result, dst);
        break;
    }
    case 7: { // Flip
        QString mode = params.value("mode", "H").toString();
        const cv::Mat srcView = OpenCvImageBridge::constMatView(src);
        cv::Mat dstView = OpenCvImageBridge::mutableMatView(dst);
        if (srcView.empty() || dstView.empty())
            break;

        int flipCode = -1;
        if (mode == "H")
            flipCode = 1;
        else if (mode == "V")
            flipCode = 0;
        cv::flip(srcView, dstView, flipCode);
        break;
    }
    case 8: { // Rotate
        double degree = params.value("degree", 30.0).toDouble();
        double angleRad = -degree * M_PI / 180.0;
        double cosA = std::cos(angleRad), sinA = std::sin(angleRad);
        auto normalizeTrig = [](double v) {
            if (std::abs(v) < 1e-12)
                return 0.0;
            if (std::abs(v - 1.0) < 1e-12)
                return 1.0;
            if (std::abs(v + 1.0) < 1e-12)
                return -1.0;
            return v;
        };
        cosA = normalizeTrig(cosA);
        sinA = normalizeTrig(sinA);
        auto forwardX = [&](double x, double y, double cx, double cy) {
            double tx = x - cx, ty = y - cy;
            return tx * cosA + ty * sinA;
        };
        auto forwardY = [&](double x, double y, double cx, double cy) {
            double tx = x - cx, ty = y - cy;
            return -tx * sinA + ty * cosA;
        };

        double srcCx = (W - 1) / 2.0, srcCy = (H - 1) / 2.0;
        double minBx = std::numeric_limits<double>::max();
        double minBy = std::numeric_limits<double>::max();
        double maxBx = std::numeric_limits<double>::lowest();
        double maxBy = std::numeric_limits<double>::lowest();
        bool hasBounds = false;

        auto includeCorner = [&](double x, double y) {
            double rx = forwardX(x, y, srcCx, srcCy);
            double ry = forwardY(x, y, srcCx, srcCy);
            minBx = std::min(minBx, rx);
            minBy = std::min(minBy, ry);
            maxBx = std::max(maxBx, rx);
            maxBy = std::max(maxBy, ry);
        };

        if (src.channels >= 4) {
            int minX = W, minY = H, maxX = -1, maxY = -1;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    if (px(src, x, y, 3) != 0) {
                        minX = std::min(minX, x);
                        minY = std::min(minY, y);
                        maxX = std::max(maxX, x);
                        maxY = std::max(maxY, y);
                    }

            if (maxX >= minX && maxY >= minY) {
                srcCx = (minX + maxX) / 2.0;
                srcCy = (minY + maxY) / 2.0;
                for (int y = minY; y <= maxY; ++y)
                    for (int x = minX; x <= maxX; ++x)
                        if (px(src, x, y, 3) != 0) {
                            includeCorner(x - 0.5, y - 0.5);
                            includeCorner(x + 0.5, y - 0.5);
                            includeCorner(x - 0.5, y + 0.5);
                            includeCorner(x + 0.5, y + 0.5);
                            hasBounds = true;
                        }
            }
        }

        if (!hasBounds) {
            includeCorner(-0.5, -0.5);
            includeCorner(W - 0.5, -0.5);
            includeCorner(-0.5, H - 0.5);
            includeCorner(W - 0.5, H - 0.5);
        }

        int outW = std::max(1, static_cast<int>(std::ceil(maxBx - minBx)));
        int outH = std::max(1, static_cast<int>(std::ceil(maxBy - minBy)));

        // Always output RGBA: corners outside source stay (0,0,0,0) = transparent,
        // letting the viewport shader composite them over the background colour.
        dst.width = outW;
        dst.height = outH;
        dst.channels = 4;
        dst.data.assign(outW * outH * 4, 0);

        const cv::Mat srcRgba = rgbaGeometryMat(src);
        cv::Mat dstView = OpenCvImageBridge::mutableMatView(dst);
        if (srcRgba.empty() || dstView.empty())
            break;

        const double offsetX = minBx + 0.5;
        const double offsetY = minBy + 0.5;
        const cv::Matx23d dstToSrc(
            cosA, -sinA, offsetX * cosA - offsetY * sinA + srcCx,
            sinA,  cosA, offsetX * sinA + offsetY * cosA + srcCy);
        cv::warpAffine(srcRgba, dstView, dstToSrc, cv::Size(outW, outH),
                       cv::INTER_NEAREST | cv::WARP_INVERSE_MAP,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 0));
        break;
    }
    case 9: { // Emboss
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        const cv::Mat kernel = (cv::Mat_<double>(3, 3) <<
            -1.0, 0.0, 0.0,
             0.0, 0.0, 0.0,
             0.0, 0.0, 1.0);
        writeRgbChannels(filterRgb(rgb, kernel, 128.0), dst);
        break;
    }
    case 10: { // Contrast Stretch
        int stat_min = params.value("stat_min", 0).toInt();
        int stat_max = params.value("stat_max", 255).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        writeRgbChannels(stretchRgbWithStats(rgb, stat_min, stat_max, true), dst);
        break;
    }
    case 11: { // 3x3 Blur
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat blurred;
        cv::blur(rgb, blurred, cv::Size(3, 3), cv::Point(-1, -1),
                 cv::BORDER_REPLICATE);
        writeRgbChannels(blurred, dst);
        break;
    }
    case 12: { // 5x5 Blur
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat blurred;
        cv::blur(rgb, blurred, cv::Size(5, 5), cv::Point(-1, -1),
                 cv::BORDER_REPLICATE);
        writeRgbChannels(blurred, dst);
        break;
    }
    case 13: { // Gaussian Blur
        int kernelSize = params.value("kernel", "5").toString().toInt();
        if (kernelSize != 3 && kernelSize != 5 && kernelSize != 7) kernelSize = 5;
        double sigma = params.value("sigma", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        writeRgbChannels(gaussianBlurRgb(rgb, kernelSize, sigma), dst);
        break;
    }
    case 14: { // Sharpen (Unsharp Mask)
        double alpha = params.value("alpha", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat rgb64, blurred64, sharpened64;
        rgb.convertTo(rgb64, CV_64F);
        gaussianBlurRgb(rgb, 5, 1.0).convertTo(blurred64, CV_64F);
        cv::addWeighted(rgb64, 1.0 + alpha, blurred64, -alpha, 0.0,
                        sharpened64);
        cv::Mat sharpened;
        sharpened64.convertTo(sharpened, CV_8U);
        writeRgbChannels(sharpened, dst);
        break;
    }
    case 15: { // High-Pass Sharpen
        double strength = params.value("strength", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        const cv::Mat kernel = (cv::Mat_<double>(3, 3) <<
            -1.0, -1.0, -1.0,
            -1.0,  8.0, -1.0,
            -1.0, -1.0, -1.0);
        cv::Mat detail64;
        cv::filter2D(rgb, detail64, CV_64F, kernel, cv::Point(-1, -1), 0.0,
                     cv::BORDER_REPLICATE);

        cv::Mat rgb64, sharpened64;
        rgb.convertTo(rgb64, CV_64F);
        cv::addWeighted(rgb64, 1.0, detail64, strength, 0.0, sharpened64);
        cv::Mat sharpened;
        sharpened64.convertTo(sharpened, CV_8U);
        writeRgbChannels(sharpened, dst);
        break;
    }
    case 16: { // High-Boost
        double beta = params.value("beta", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat rgb64, blurred64, boosted64;
        rgb.convertTo(rgb64, CV_64F);
        gaussianBlurRgb(rgb, 5, 1.0).convertTo(blurred64, CV_64F);
        cv::addWeighted(rgb64, 1.0 + beta, blurred64, -1.0, 0.0, boosted64);
        cv::Mat boosted;
        boosted64.convertTo(boosted, CV_8U);
        writeRgbChannels(boosted, dst);
        break;
    }
    case 17: { // Diagonal Motion Blur
        int distance = params.value("distance", 9).toInt();
        if (distance < 1)
            distance = 1;
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat kernel = cv::Mat::zeros(distance, distance, CV_64F);
        for (int i = 0; i < distance; ++i)
            kernel.at<double>(i, i) = 1.0 / distance;
        writeRgbChannels(filterRgb(rgb, kernel), dst);
        break;
    }
    case 18: { // Horizontal Motion Blur
        int distance = params.value("distance", 9).toInt();
        if (distance < 1)
            distance = 1;
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat kernel = cv::Mat::ones(1, distance, CV_64F) / distance;
        writeRgbChannels(filterRgb(rgb, kernel), dst);
        break;
    }
    case 19: { // Horizontal Edge (Sobel Gy)
        double scale = params.value("scale", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        const cv::Mat kernel = (cv::Mat_<double>(3, 3) <<
            -1.0, -2.0, -1.0,
             0.0,  0.0,  0.0,
             1.0,  2.0,  1.0);
        cv::Mat edge64;
        cv::filter2D(luminanceMat(rgb), edge64, CV_64F, kernel, cv::Point(-1, -1), 0.0,
                     cv::BORDER_REPLICATE);
        cv::Mat edge;
        cv::Mat(cv::abs(edge64) * scale).convertTo(edge, CV_8U);
        writeRgbChannels(mergeGrayToRgb(edge), dst);
        break;
    }
    case 20: { // Vertical Edge (Sobel Gx)
        double scale = params.value("scale", 1.0).toDouble();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        const cv::Mat kernel = (cv::Mat_<double>(3, 3) <<
            -1.0, 0.0, 1.0,
            -2.0, 0.0, 2.0,
            -1.0, 0.0, 1.0);
        cv::Mat edge64;
        cv::filter2D(luminanceMat(rgb), edge64, CV_64F, kernel, cv::Point(-1, -1), 0.0,
                     cv::BORDER_REPLICATE);
        cv::Mat edge;
        cv::Mat(cv::abs(edge64) * scale).convertTo(edge, CV_8U);
        writeRgbChannels(mergeGrayToRgb(edge), dst);
        break;
    }
    case 21: { // Laplacian
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        const cv::Mat kernel = (cv::Mat_<double>(3, 3) <<
             0.0, -1.0,  0.0,
            -1.0,  4.0, -1.0,
             0.0, -1.0,  0.0);
        writeRgbChannels(filterRgb(rgb, kernel, 128.0), dst);
        break;
    }
    case 22: { // DoG
        double sigma1 = params.value("sigma1", 1.0).toDouble();
        double sigma2 = params.value("sigma2", 2.0).toDouble();
        double gain = params.value("gain", 1.0).toDouble();
        auto kernelSize = [](double sigma) {
            int k = std::max(3, 2 * static_cast<int>(3.0 * sigma) + 1);
            if (k % 2 == 0) k++;
            return k;
        };
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat blur164, blur264, dog64;
        gaussianBlurRgb(rgb, kernelSize(sigma1), sigma1).convertTo(blur164, CV_64F);
        gaussianBlurRgb(rgb, kernelSize(sigma2), sigma2).convertTo(blur264, CV_64F);
        cv::addWeighted(blur164, gain, blur264, -gain, 128.0, dog64);
        cv::Mat dog;
        dog64.convertTo(dog, CV_8U);
        writeRgbChannels(dog, dst);
        break;
    }
    case 23: { // Histogram Stretch
        int stat_hmin = params.value("stat_hmin", 0).toInt();
        int stat_hmax = params.value("stat_hmax", 255).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        writeRgbChannels(stretchRgbWithStats(rgb, stat_hmin, stat_hmax, false), dst);
        break;
    }
    case 24: { // Endpoint Detection
        int threshold = params.value("threshold", 127).toInt();
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat foregroundMask;
        cv::compare(luminanceMat(rgb), threshold, foregroundMask, cv::CMP_GE);

        cv::Mat foreground01;
        foregroundMask.convertTo(foreground01, CV_8U, 1.0 / 255.0);
        cv::Mat padded;
        cv::copyMakeBorder(foreground01, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar(0));

        cv::Mat neighborCount = cv::Mat::zeros(foreground01.size(), CV_16U);
        for (int dy = 0; dy < 3; ++dy)
            for (int dx = 0; dx < 3; ++dx) {
                if (dx == 1 && dy == 1)
                    continue;
                cv::Mat shifted16;
                padded(cv::Rect(dx, dy, W, H)).convertTo(shifted16, CV_16U);
                cv::add(neighborCount, shifted16, neighborCount, cv::noArray(), CV_16U);
            }

        cv::Mat singleNeighborMask;
        cv::compare(neighborCount, 1, singleNeighborMask, cv::CMP_EQ);
        cv::Mat endpoints;
        cv::bitwise_and(singleNeighborMask, foregroundMask, endpoints);
        writeRgbChannels(mergeGrayToRgb(endpoints), dst);
        break;
    }
    case 25: { // Median Smoothing
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat smoothed;
        cv::medianBlur(rgb, smoothed, 3);
        writeRgbChannels(smoothed, dst);
        break;
    }
    case 26: { // Grayscale Average
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat rgb64;
        rgb.convertTo(rgb64, CV_64F);
        cv::Mat gray64;
        cv::transform(rgb64, gray64,
                      cv::Matx<double, 1, 3>(1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0));
        cv::Mat gray;
        gray64.convertTo(gray, CV_8U);
        writeRgbChannels(mergeGrayToRgb(gray), dst);
        break;
    }
    case 27: { // Grayscale Luminosity
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        cv::Mat gray;
        luminanceMat(rgb).convertTo(gray, CV_8U);
        writeRgbChannels(mergeGrayToRgb(gray), dst);
        break;
    }
    case 28: { // Grayscale Lightness
        const cv::Mat rgb = rgbChannelsMat(src);
        if (rgb.empty())
            break;
        std::vector<cv::Mat> channels;
        cv::split(rgb, channels);

        cv::Mat maxRg;
        cv::Mat maxRgb;
        cv::Mat minRg;
        cv::Mat minRgb;
        cv::max(channels[0], channels[1], maxRg);
        cv::max(maxRg, channels[2], maxRgb);
        cv::min(channels[0], channels[1], minRg);
        cv::min(minRg, channels[2], minRgb);

        cv::Mat gray;
        cv::addWeighted(maxRgb, 0.5, minRgb, 0.5, 0.0, gray);
        writeRgbChannels(mergeGrayToRgb(gray), dst);
        break;
    }
    }

    clearTransparentRgb(dst);
    return true;
}

double ImageProcessorCore::computeAverageLuminance(const ImageBuffer& src) {
    double sum = 0.0;
    int count = 0;
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            uint8_t r = px(src, x, y, 0);
            uint8_t g = px(src, x, y, 1);
            uint8_t b = px(src, x, y, 2);
            sum += lum(r, g, b);
            count++;
        }
    }
    return count > 0 ? sum / count : 0.0;
}

std::pair<uint8_t, uint8_t> ImageProcessorCore::computeMinMax(const ImageBuffer& src) {
    uint8_t minv = 255, maxv = 0;
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            for (int c = 0; c < 3; ++c) {
                uint8_t v = px(src, x, y, c);
                minv = std::min(minv, v);
                maxv = std::max(maxv, v);
            }
        }
    }
    return {minv, maxv};
}

std::array<int, 256> ImageProcessorCore::computeLuminanceHistogram(const ImageBuffer& src) {
    std::array<int, 256> hist = {};
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            uint8_t r = px(src, x, y, 0);
            uint8_t g = px(src, x, y, 1);
            uint8_t b = px(src, x, y, 2);
            double l = lum(r, g, b);
            int idx = static_cast<int>(std::round(l));
            idx = std::max(0, std::min(255, idx));
            hist[idx]++;
        }
    }
    return hist;
}
