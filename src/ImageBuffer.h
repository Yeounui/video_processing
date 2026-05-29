#pragma once

#include <cstdint>
#include <vector>

struct ImageBuffer {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int channels = 3;
};

struct ContentRect {
    float x = 0, y = 0, w = 0, h = 0;
};

enum class SourceType {
    SOURCE_NONE,
    SOURCE_IMAGE,
    SOURCE_VIDEO_FILE,
    SOURCE_REALTIME_STREAM
};
