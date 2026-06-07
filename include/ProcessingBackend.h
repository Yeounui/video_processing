#pragma once

#include <QVariantMap>
#include <vector>

class ProcessingBackend {
public:
    enum class Kind {
        CpuReference,
        OpenGl
    };

    struct Effect {
        int algorithmId = 0;
        QVariantMap params;
    };

    struct StackPlan {
        int cpuPrefixCount = 0;
        std::vector<Effect> acceleratedSuffix;
        bool suffixCanFuse = false;

        bool hasAcceleratedSuffix() const { return !acceleratedSuffix.empty(); }
    };

    static bool supportsAcceleratedAlgorithm(int algorithmId, Kind kind = Kind::OpenGl);
    static bool supportsFusedAlgorithm(int algorithmId, Kind kind = Kind::OpenGl);
    static bool supportsFusedStack(const std::vector<Effect> &effects,
                                   Kind kind = Kind::OpenGl);
    static bool requiresCpuStatistics(int algorithmId);
    static void setRuntimeAcceleratedBackendAvailable(bool available);
    static void clearRuntimeAcceleratedBackendAvailability();
    static bool runtimeAcceleratedBackendAvailable();
    static Kind defaultKindFromEnvironment();
    static StackPlan planVideoStack(const std::vector<Effect> &effects,
                                    Kind kind = Kind::OpenGl);
};
