#include "ProcessingBackend.h"

#include <QString>
#include <algorithm>

namespace {
constexpr auto kProcessingBackendEnv = "QT_UI_PROCESSING_BACKEND";

bool containsAlgorithm(const int *begin, const int *end, int algorithmId)
{
    return std::find(begin, end, algorithmId) != end;
}
}

bool ProcessingBackend::supportsAcceleratedAlgorithm(int algorithmId, Kind kind)
{
    if (kind != Kind::OpenGl) {
        return false;
    }

    static const int supported[] = {
        1, 2, 3, 4, 6, 7, 9, 11, 12, 14, 19, 20, 21, 25, 26, 27, 28
    };
    return containsAlgorithm(std::begin(supported), std::end(supported), algorithmId);
}

bool ProcessingBackend::supportsFusedAlgorithm(int algorithmId, Kind kind)
{
    if (kind != Kind::OpenGl) {
        return false;
    }

    static const int supported[] = {1, 2, 3, 4, 7, 26, 27, 28};
    return containsAlgorithm(std::begin(supported), std::end(supported), algorithmId);
}

bool ProcessingBackend::supportsFusedStack(const std::vector<Effect> &effects, Kind kind)
{
    if (effects.empty() || effects.size() > 3) {
        return false;
    }

    return std::all_of(effects.begin(), effects.end(), [kind](const Effect &effect) {
        return supportsFusedAlgorithm(effect.algorithmId, kind);
    });
}

bool ProcessingBackend::requiresCpuStatistics(int algorithmId)
{
    return algorithmId == 5 || algorithmId == 10 || algorithmId == 23;
}

ProcessingBackend::Kind ProcessingBackend::defaultKindFromEnvironment()
{
    const QString value = qEnvironmentVariable(kProcessingBackendEnv).trimmed().toLower();
    if (value == QStringLiteral("cpu")
        || value == QStringLiteral("cpu-reference")
        || value == QStringLiteral("cpureference")) {
        return Kind::CpuReference;
    }

    return Kind::OpenGl;
}

ProcessingBackend::StackPlan ProcessingBackend::planVideoStack(
    const std::vector<Effect> &effects, Kind kind)
{
    StackPlan plan;
    int suffixStart = static_cast<int>(effects.size());

    for (int i = static_cast<int>(effects.size()) - 1; i >= 0; --i) {
        if (!supportsAcceleratedAlgorithm(effects[static_cast<std::size_t>(i)].algorithmId, kind)) {
            break;
        }
        suffixStart = i;
    }

    plan.cpuPrefixCount = suffixStart;
    if (suffixStart < static_cast<int>(effects.size())) {
        plan.acceleratedSuffix.assign(effects.begin() + suffixStart, effects.end());
        plan.suffixCanFuse = supportsFusedStack(plan.acceleratedSuffix, kind);
    }
    return plan;
}
