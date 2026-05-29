#pragma once

#include "ImageBuffer.h"
#include <memory>
#include <QString>

class ImageIoService {
public:
    static std::shared_ptr<ImageBuffer> load(const QString &path);
    static bool save(const ImageBuffer &buf, const QString &path);
};
