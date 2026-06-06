#pragma once
#include "ImageBuffer.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QVariantMap>
#include <memory>
#include <unordered_map>
#include <vector>

struct GpuEffectCommand {
    int algorithmId = 0;
    QVariantMap params;
};

class GpuEffectPipeline {
public:
    ~GpuEffectPipeline() { destroy(); }

    // Call when GL context is current (called from render())
    bool initialize();
    // Call from releaseResources()
    void destroy();
    bool isInitialized() const { return initialized_; }

    // Run GPU effect on src, return CPU readback result (nullptr on failure)
    std::shared_ptr<ImageBuffer> applyStaticEffect(
        const ImageBuffer &src, int algorithmId, const QVariantMap &params);

    // Run a video/display effect stack fully on the GPU and return the output texture.
    // The returned texture is owned by this pipeline and remains valid until the next
    // stack render, resize, or destroy().
    GLuint applyEffectStackToTexture(
        const ImageBuffer &src, const std::vector<GpuEffectCommand> &effects);

    // Static helper: true if algorithmId has a GLSL implementation
    static bool supportsAlgorithm(int algorithmId);
    static bool supportsFusedAlgorithm(int algorithmId);
    static bool supportsFusedStack(const std::vector<GpuEffectCommand> &effects);

private:
    QOpenGLFunctions_3_3_Core gl_;
    bool initialized_ = false;

    GLuint vao_ = 0, vbo_ = 0;
    GLuint srcTex_ = 0, dstTex_ = 0, auxTex_ = 0, fbo_ = 0;
    int cachedW_ = 0, cachedH_ = 0;

    struct Program { GLuint id = 0; bool failed = false; };
    std::unordered_map<int, Program> programs_;
    Program fusedProgram_;

    GLuint getOrCompileProgram(int algorithmId);
    GLuint getOrCompileFusedProgram();
    bool ensureTextures(int w, int h);
    void uploadSrc(const ImageBuffer &src);
    std::shared_ptr<ImageBuffer> readback(int w, int h, int channels);
    void setUniforms(GLuint prog, int algorithmId, const QVariantMap &params,
                     int w, int h, GLuint inputTex);
    void setFusedUniforms(GLuint prog, const std::vector<GpuEffectCommand> &effects,
                          int w, int h, GLuint inputTex);
    void drawFullscreen(GLuint prog, int w, int h, GLuint targetTex);

    static const char *fragmentSource(int algorithmId);
    static const char *fusedFragmentSource();
};
