#ifndef VIDEO_PROCESSING_TYPES_H
#define VIDEO_PROCESSING_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_EFFECT_STACK   3
#define EFFECT_PARAM_SLOTS 6
#define APP_LAST_ERROR_LEN 256
#define PROCESSOR_ALGORITHM_COUNT 28

typedef enum {
    SOURCE_NONE = 0,
    SOURCE_IMAGE,
    SOURCE_VIDEO_FILE,
    SOURCE_REALTIME_STREAM
} SourceType;

typedef enum {
    BACKEND_CPU = 0,
    BACKEND_GLSL,
    BACKEND_CPU_GLSL
} EffectBackend;

typedef struct {
    uint8_t *data;
    int      width;
    int      height;
    int      channels; /* always 3 */
} ImageBuffer;

typedef struct {
    unsigned int textureId;       /* GLuint */
    unsigned int fboId;           /* GLuint, 0 when not a render target */
    int          width;
    int          height;
    unsigned int internalFormat;  /* GLenum, e.g. GL_RGBA8 */
    bool         isFresh;
} GpuImage;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} ContentRect;

typedef struct {
    float values[EFFECT_PARAM_SLOTS];
} EffectParams;

typedef struct {
    int            algorithmId; /* 1..PROCESSOR_ALGORITHM_COUNT */
    EffectBackend  backend;
    EffectParams   params;
    int            passCount;
} EffectCommand;

typedef struct {
    int            algorithmId;
    EffectBackend  defaultBackend;
    int            passPlan;          /* per-algorithm pass plan id */
    bool           requiresGlobalStats;
    bool           supportsGpuFallback;
} AlgorithmSpec;

typedef struct {
    bool   playing;
    bool   loop;
    bool   at_end;        /* set true at natural EOF; cleared on seek */
    float  speed;         /* one of {0.5f, 0.75f, 1.0f, 1.5f, 2.0f} */
    double current_pts_s;
    double duration_s;    /* 0 if unknown */
} VideoState;

typedef struct {
    bool   playing;
    bool   disconnected;
    int    width;
    int    height;
    float  fps;
} RealtimeInfo;

typedef struct {
    SourceType    sourceType;

    ImageBuffer   inImage;
    ImageBuffer   outImage;
    ImageBuffer  *displayImageCpuFallback; /* NULL on default GLSL path */
    uint8_t       *geometryMask;            /* static-image valid pixels after geometric edits */
    int            geometryMaskW;
    int            geometryMaskH;

    ContentRect   contentRect;

    EffectCommand effectStack[MAX_EFFECT_STACK];
    int           effectCount;

    ImageBuffer   preRotateImage;  /* image mode: outImage state before first rotate */
    float         imgRotateAngle;  /* accumulated rotate angle (image + video mode) */
    bool          imgFlipH;        /* accumulated horizontal flip (video mode) */
    bool          imgFlipV;        /* accumulated vertical flip (video mode) */

    VideoState    video;
    RealtimeInfo  realtime;

    bool          running;
    char          lastError[APP_LAST_ERROR_LEN];
} AppState;

#endif /* VIDEO_PROCESSING_TYPES_H */
