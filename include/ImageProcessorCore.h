#pragma once

#include "ImageBuffer.h"
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <array>
#include <utility>
#include <vector>

enum class ParamType { Int, Float, Enum, Auto };
enum class ControlType { Slider, ComboBox, SpinBox };

struct ParameterSpec {
    QString key;
    QString label;
    ParamType valueType = ParamType::Float;
    ControlType controlType = ControlType::Slider;
    double min = 0.0;
    double max = 1.0;
    double step = 1.0;
    QVariant defaultValue;
    QStringList enumValues;   // for Enum type
    bool isAutomatic = false; // computed from image stats; shown read-only in UI
};

struct AlgorithmSpec {
    int id;           // 1..28
    QString name;
    QString category; // "Point" | "Geometry" | "Filter" | "Edge" | "Morphology" | "Grayscale"
    std::vector<ParameterSpec> params;
};

using EffectParams = QVariantMap;

class ImageProcessorCore {
public:
    // Returns the canonical list of all 28 AlgorithmSpecs (ids 1..28, in order).
    static const std::vector<AlgorithmSpec>& specs();

    // Apply algorithm `algorithmId` (1..28) from src to dst.
    // Most algorithms keep src dimensions; rotate expands dst to fit the rotated image.
    // For stats-dependent algorithms (#5 averageThreshold, #10 contrastStretch,
    // #23 histogramStretch), the caller must pre-compute the stat and embed it in params:
    //   #5:  params["stat_average"]  = double (average luminance)
    //   #10: params["stat_min"]      = int,  params["stat_max"] = int  (global min/max across all channels)
    //   #23: params["stat_hmin"]     = int,  params["stat_hmax"] = int (first/last non-zero luminance histogram bin)
    // Returns false if algorithmId is out of range [1..28].
    static bool apply(const ImageBuffer& src, ImageBuffer& dst,
                      int algorithmId, const EffectParams& params);

    // Stats helpers — call before apply() for algorithms #5, #10, #23.
    static double computeAverageLuminance(const ImageBuffer& src);
    // Returns {global_min, global_max} across all channels.
    static std::pair<uint8_t, uint8_t> computeMinMax(const ImageBuffer& src);
    // Returns histogram of luminance values (Y = 0.299R + 0.587G + 0.114B, rounded).
    static std::array<int, 256> computeLuminanceHistogram(const ImageBuffer& src);
};
