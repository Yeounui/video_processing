#include "ImageProcessorCore.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>

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

// 1D Gaussian kernel (normalized), size must be odd
static std::vector<double> makeGaussian1D(int size, double sigma) {
    std::vector<double> k(size);
    int half = size / 2;
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        double x = i - half;
        k[i] = std::exp(-x * x / (2.0 * sigma * sigma));
        sum += k[i];
    }
    for (auto& v : k) v /= sum;
    return k;
}

// Separable Gaussian blur: horizontal then vertical pass into dst
static void gaussianBlur(const ImageBuffer& src, ImageBuffer& dst, int kernelSize, double sigma) {
    resizeDst(src, dst);
    auto k = makeGaussian1D(kernelSize, sigma);
    int half = kernelSize / 2;
    int W = src.width, H = src.height;

    // Horizontal pass: src → tmp
    ImageBuffer tmp;
    resizeDst(src, tmp);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 3; ++c) {
                double acc = 0.0;
                for (int ki = 0; ki < kernelSize; ++ki)
                    acc += k[ki] * px(src, x + ki - half, y, c);
                setPx(tmp, x, y, c, clamp8(acc));
            }

    // Vertical pass: tmp → dst
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int c = 0; c < 3; ++c) {
                double acc = 0.0;
                for (int ki = 0; ki < kernelSize; ++ki)
                    acc += k[ki] * px(tmp, x, y + ki - half, c);
                setPx(dst, x, y, c, clamp8(acc));
            }
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
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c)
                    setPx(dst, x, y, c, clamp8(px(src, x, y, c) + delta));
        break;
    }
    case 2: { // Multiply
        double factor = params.value("factor", 1.2).toDouble();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c)
                    setPx(dst, x, y, c, clamp8(px(src, x, y, c) * factor));
        break;
    }
    case 3: { // Gamma
        double gamma = params.value("gamma", 2.2).toDouble();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    uint8_t v = px(src, x, y, c);
                    double normalized = v / 255.0;
                    double corrected = std::pow(normalized, 1.0 / gamma) * 255.0;
                    setPx(dst, x, y, c, clamp8(corrected));
                }
        break;
    }
    case 4: { // Fixed Threshold
        int threshold = params.value("threshold", 127).toInt();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double l = lum(px(src, x, y, 0), px(src, x, y, 1), px(src, x, y, 2));
                uint8_t v = (l >= threshold) ? 255 : 0;
                setPx(dst, x, y, 0, v);
                setPx(dst, x, y, 1, v);
                setPx(dst, x, y, 2, v);
            }
        break;
    }
    case 5: { // Average Threshold
        double average = params.value("stat_average", 128).toDouble();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double l = lum(px(src, x, y, 0), px(src, x, y, 1), px(src, x, y, 2));
                uint8_t v = (l >= average) ? 255 : 0;
                setPx(dst, x, y, 0, v);
                setPx(dst, x, y, 1, v);
                setPx(dst, x, y, 2, v);
            }
        break;
    }
    case 6: { // Bitwise AND
        int mask = params.value("mask", 0xC9).toInt();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c)
                    setPx(dst, x, y, c, px(src, x, y, c) & mask);
        break;
    }
    case 7: { // Flip
        QString mode = params.value("mode", "H").toString();
        if (mode == "H") {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    for (int c = 0; c < src.channels; ++c)
                        setPx(dst, x, y, c, px(src, W - 1 - x, y, c));
        } else if (mode == "V") {
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    for (int c = 0; c < src.channels; ++c)
                        setPx(dst, x, y, c, px(src, x, H - 1 - y, c));
        } else { // "Both"
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    for (int c = 0; c < src.channels; ++c)
                        setPx(dst, x, y, c, px(src, W - 1 - x, H - 1 - y, c));
        }
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

        for (int dy = 0; dy < outH; ++dy) {
            for (int dx = 0; dx < outW; ++dx) {
                double tx = minBx + dx + 0.5;
                double ty = minBy + dy + 0.5;
                double sx = tx * cosA - ty * sinA + srcCx;
                double sy = tx * sinA + ty * cosA + srcCy;
                int sx_i = static_cast<int>(std::round(sx));
                int sy_i = static_cast<int>(std::round(sy));
                if (sx_i >= 0 && sx_i < W && sy_i >= 0 && sy_i < H) {
                    for (int c = 0; c < 3; ++c)
                        setPx(dst, dx, dy, c, px(src, sx_i, sy_i, c));
                    uint8_t a = (src.channels >= 4) ? px(src, sx_i, sy_i, 3) : 255;
                    setPx(dst, dx, dy, 3, a);
                }
            }
        }
        break;
    }
    case 9: { // Emboss
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = px(src, x + 1, y + 1, c) - px(src, x - 1, y - 1, c) + 128.0;
                    setPx(dst, x, y, c, clamp8(v));
                }
        break;
    }
    case 10: { // Contrast Stretch
        int stat_min = params.value("stat_min", 0).toInt();
        int stat_max = params.value("stat_max", 255).toInt();
        if (stat_min == stat_max) {
            for (int i = 0; i < (int)src.data.size(); ++i)
                dst.data[i] = src.data[i];
        } else {
            // Use integer arithmetic where possible to minimize floating point error
            int denom = stat_max - stat_min;
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    for (int c = 0; c < 3; ++c) {
                        int v = px(src, x, y, c);
                        // (v - min) * 255 / denom
                        double stretched = ((v - stat_min) * 255.0) / denom;
                        setPx(dst, x, y, c, clamp8(stretched));
                    }
        }
        break;
    }
    case 11: { // 3x3 Blur
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double acc = 0.0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            acc += px(src, x + dx, y + dy, c);
                    setPx(dst, x, y, c, clamp8(acc / 9.0));
                }
        break;
    }
    case 12: { // 5x5 Blur
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double acc = 0.0;
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dx = -2; dx <= 2; ++dx)
                            acc += px(src, x + dx, y + dy, c);
                    setPx(dst, x, y, c, clamp8(acc / 25.0));
                }
        break;
    }
    case 13: { // Gaussian Blur
        int kernelSize = params.value("kernel", "5").toString().toInt();
        if (kernelSize != 3 && kernelSize != 5 && kernelSize != 7) kernelSize = 5;
        double sigma = params.value("sigma", 1.0).toDouble();
        gaussianBlur(src, dst, kernelSize, sigma);
        break;
    }
    case 14: { // Sharpen (Unsharp Mask)
        double alpha = params.value("alpha", 1.0).toDouble();
        ImageBuffer blurred;
        gaussianBlur(src, blurred, 5, 1.0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = px(src, x, y, c) + alpha * (px(src, x, y, c) - px(blurred, x, y, c));
                    setPx(dst, x, y, c, clamp8(v));
                }
        break;
    }
    case 15: { // High-Pass Sharpen
        double strength = params.value("strength", 1.0).toDouble();
        // Store signed raw high-pass values in double buffer (avoids UB from uint8_t scratch)
        std::vector<double> detail(W * H * 3);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double acc = -px(src, x - 1, y - 1, c) - px(src, x, y - 1, c) - px(src, x + 1, y - 1, c)
                                - px(src, x - 1, y, c) + 8.0 * px(src, x, y, c) - px(src, x + 1, y, c)
                                - px(src, x - 1, y + 1, c) - px(src, x, y + 1, c) - px(src, x + 1, y + 1, c);
                    detail[(y * W + x) * 3 + c] = acc;
                }
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = px(src, x, y, c) + strength * detail[(y * W + x) * 3 + c];
                    setPx(dst, x, y, c, clamp8(v));
                }
        break;
    }
    case 16: { // High-Boost
        double beta = params.value("beta", 1.0).toDouble();
        ImageBuffer blurred;
        gaussianBlur(src, blurred, 5, 1.0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = (1.0 + beta) * px(src, x, y, c) - px(blurred, x, y, c);
                    setPx(dst, x, y, c, clamp8(v));
                }
        break;
    }
    case 17: { // Diagonal Motion Blur
        int distance = params.value("distance", 9).toInt();
        int half = distance / 2;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double acc = 0.0;
                    for (int i = 0; i < distance; ++i)
                        acc += px(src, x + i - half, y + i - half, c);
                    setPx(dst, x, y, c, clamp8(acc / distance));
                }
        break;
    }
    case 18: { // Horizontal Motion Blur
        int distance = params.value("distance", 9).toInt();
        int half = distance / 2;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double acc = 0.0;
                    for (int i = 0; i < distance; ++i)
                        acc += px(src, x + i - half, y, c);
                    setPx(dst, x, y, c, clamp8(acc / distance));
                }
        break;
    }
    case 19: { // Horizontal Edge (Sobel Gy)
        double scale = params.value("scale", 1.0).toDouble();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double gy = -lum(px(src, x - 1, y - 1, 0), px(src, x - 1, y - 1, 1), px(src, x - 1, y - 1, 2))
                          - 2.0 * lum(px(src, x, y - 1, 0), px(src, x, y - 1, 1), px(src, x, y - 1, 2))
                          - lum(px(src, x + 1, y - 1, 0), px(src, x + 1, y - 1, 1), px(src, x + 1, y - 1, 2))
                          + lum(px(src, x - 1, y + 1, 0), px(src, x - 1, y + 1, 1), px(src, x - 1, y + 1, 2))
                          + 2.0 * lum(px(src, x, y + 1, 0), px(src, x, y + 1, 1), px(src, x, y + 1, 2))
                          + lum(px(src, x + 1, y + 1, 0), px(src, x + 1, y + 1, 1), px(src, x + 1, y + 1, 2));
                uint8_t edge = clamp8(std::abs(gy) * scale);
                setPx(dst, x, y, 0, edge);
                setPx(dst, x, y, 1, edge);
                setPx(dst, x, y, 2, edge);
            }
        break;
    }
    case 20: { // Vertical Edge (Sobel Gx)
        double scale = params.value("scale", 1.0).toDouble();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                double gx = -lum(px(src, x - 1, y - 1, 0), px(src, x - 1, y - 1, 1), px(src, x - 1, y - 1, 2))
                          + lum(px(src, x + 1, y - 1, 0), px(src, x + 1, y - 1, 1), px(src, x + 1, y - 1, 2))
                          - 2.0 * lum(px(src, x - 1, y, 0), px(src, x - 1, y, 1), px(src, x - 1, y, 2))
                          + 2.0 * lum(px(src, x + 1, y, 0), px(src, x + 1, y, 1), px(src, x + 1, y, 2))
                          - lum(px(src, x - 1, y + 1, 0), px(src, x - 1, y + 1, 1), px(src, x - 1, y + 1, 2))
                          + lum(px(src, x + 1, y + 1, 0), px(src, x + 1, y + 1, 1), px(src, x + 1, y + 1, 2));
                uint8_t edge = clamp8(std::abs(gx) * scale);
                setPx(dst, x, y, 0, edge);
                setPx(dst, x, y, 1, edge);
                setPx(dst, x, y, 2, edge);
            }
        break;
    }
    case 21: { // Laplacian
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = -px(src, x, y - 1, c) - px(src, x - 1, y, c) + 4.0 * px(src, x, y, c)
                             - px(src, x + 1, y, c) - px(src, x, y + 1, c) + 128.0;
                    setPx(dst, x, y, c, clamp8(v));
                }
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
        ImageBuffer blur1, blur2;
        gaussianBlur(src, blur1, kernelSize(sigma1), sigma1);
        gaussianBlur(src, blur2, kernelSize(sigma2), sigma2);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    double v = gain * (px(blur1, x, y, c) - px(blur2, x, y, c)) + 128.0;
                    setPx(dst, x, y, c, clamp8(v));
                }
        break;
    }
    case 23: { // Histogram Stretch
        int stat_hmin = params.value("stat_hmin", 0).toInt();
        int stat_hmax = params.value("stat_hmax", 255).toInt();
        if (stat_hmin == stat_hmax) {
            for (int i = 0; i < (int)src.data.size(); ++i)
                dst.data[i] = src.data[i];
        } else {
            double scale = 255.0 / (stat_hmax - stat_hmin);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    for (int c = 0; c < 3; ++c) {
                        uint8_t v = px(src, x, y, c);
                        double stretched = (v - stat_hmin) * scale;
                        setPx(dst, x, y, c, clamp8(stretched));
                    }
        }
        break;
    }
    case 24: { // Endpoint Detection
        int threshold = params.value("threshold", 127).toInt();
        auto foreground = [&](int x, int y) {
            if (x < 0 || x >= W || y < 0 || y >= H) return false;
            return lum(px(src, x, y, 0), px(src, x, y, 1), px(src, x, y, 2)) >= threshold;
        };
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                if (!foreground(x, y)) {
                    setPx(dst, x, y, 0, 0);
                    setPx(dst, x, y, 1, 0);
                    setPx(dst, x, y, 2, 0);
                } else {
                    int count = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            if (foreground(x + dx, y + dy)) count++;
                        }
                    uint8_t v = (count == 1) ? 255 : 0;
                    setPx(dst, x, y, 0, v);
                    setPx(dst, x, y, 1, v);
                    setPx(dst, x, y, 2, v);
                }
            }
        break;
    }
    case 25: { // Median Smoothing
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                for (int c = 0; c < 3; ++c) {
                    std::vector<uint8_t> vals;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            vals.push_back(px(src, x + dx, y + dy, c));
                    std::sort(vals.begin(), vals.end());
                    setPx(dst, x, y, c, vals[4]);
                }
        break;
    }
    case 26: { // Grayscale Average
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                uint8_t r = px(src, x, y, 0);
                uint8_t g = px(src, x, y, 1);
                uint8_t b = px(src, x, y, 2);
                uint8_t g_val = clamp8((r + g + b) / 3.0);
                setPx(dst, x, y, 0, g_val);
                setPx(dst, x, y, 1, g_val);
                setPx(dst, x, y, 2, g_val);
            }
        break;
    }
    case 27: { // Grayscale Luminosity
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                uint8_t r = px(src, x, y, 0);
                uint8_t g = px(src, x, y, 1);
                uint8_t b = px(src, x, y, 2);
                uint8_t g_val = clamp8(lum(r, g, b));
                setPx(dst, x, y, 0, g_val);
                setPx(dst, x, y, 1, g_val);
                setPx(dst, x, y, 2, g_val);
            }
        break;
    }
    case 28: { // Grayscale Lightness
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                uint8_t r = px(src, x, y, 0);
                uint8_t g = px(src, x, y, 1);
                uint8_t b = px(src, x, y, 2);
                uint8_t maxv = std::max({r, g, b});
                uint8_t minv = std::min({r, g, b});
                uint8_t g_val = clamp8((maxv + minv) / 2.0);
                setPx(dst, x, y, 0, g_val);
                setPx(dst, x, y, 1, g_val);
                setPx(dst, x, y, 2, g_val);
            }
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
