#ifndef VIDEO_PROCESSING_IMAGE_PROCESSOR_H
#define VIDEO_PROCESSING_IMAGE_PROCESSOR_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

typedef enum {
    FLIP_HORIZONTAL = 0,
    FLIP_VERTICAL   = 1,
    FLIP_BOTH       = 2
} FlipMode;

typedef struct {
    uint8_t *scratch;
    size_t   scratchBytes;
    /* Phase 7: GPU effect resources */
    unsigned int effectSrcTex;
    unsigned int effectDstTex;
    unsigned int effectFbo;
    int          effectFboW;
    int          effectFboH;
    unsigned int effectVao;
    unsigned int effectVbo;
    unsigned int effectPrograms[PROCESSOR_ALGORITHM_COUNT + 1]; /* [alg_id] = compiled program; [0] unused */
    bool         gpuReady;
} ImageProcessor;

bool processor_init(ImageProcessor *p);
void processor_destroy(ImageProcessor *p);
void processor_clear_scratch(ImageProcessor *p);

/* Each algorithm reads work->data, writes to scratch, then swaps the two
 * data pointers. Caller continues to own the (now-swapped) work buffer.
 * Returns false on OOM; on failure work is left unchanged. */
bool processor_brightness(ImageProcessor *p, ImageBuffer *work, int delta);
bool processor_multiply  (ImageProcessor *p, ImageBuffer *work, float factor);
bool processor_gamma     (ImageProcessor *p, ImageBuffer *work, float gamma);
bool processor_threshold_fixed(ImageProcessor *p, ImageBuffer *work, int threshold);
bool processor_threshold_average(ImageProcessor *p, ImageBuffer *work);
bool processor_grayscale_average(ImageProcessor *p, ImageBuffer *work);
bool processor_grayscale_luminosity(ImageProcessor *p, ImageBuffer *work);
bool processor_grayscale_lightness(ImageProcessor *p, ImageBuffer *work);
bool processor_bitwise_and(ImageProcessor *p, ImageBuffer *work, unsigned int mask);
bool processor_flip      (ImageProcessor *p, ImageBuffer *work, FlipMode mode);
bool processor_rotate    (ImageProcessor *p, ImageBuffer *work, float degrees);
bool processor_rotate_expand(ImageProcessor *p, ImageBuffer *work, float degrees);
bool processor_rotate_expand_masked(ImageProcessor *p, ImageBuffer *work,
                                    uint8_t **mask, int *mask_w, int *mask_h,
                                    float degrees);
bool processor_emboss    (ImageProcessor *p, ImageBuffer *work);
bool processor_contrast_stretch(ImageProcessor *p, ImageBuffer *work);
bool processor_blur3x3   (ImageProcessor *p, ImageBuffer *work);
bool processor_blur5x5   (ImageProcessor *p, ImageBuffer *work);
bool processor_gaussian_blur(ImageProcessor *p, ImageBuffer *work, int kernel_size, float sigma);
bool processor_sharpen   (ImageProcessor *p, ImageBuffer *work, float alpha);
bool processor_high_pass_sharpen(ImageProcessor *p, ImageBuffer *work, float strength);
bool processor_high_boost(ImageProcessor *p, ImageBuffer *work, float beta);
bool processor_motion_blur_diagonal(ImageProcessor *p, ImageBuffer *work, int distance);
bool processor_motion_blur_horizontal(ImageProcessor *p, ImageBuffer *work, int distance);
bool processor_sobel_horizontal(ImageProcessor *p, ImageBuffer *work, float scale);
bool processor_sobel_vertical(ImageProcessor *p, ImageBuffer *work, float scale);
bool processor_laplacian(ImageProcessor *p, ImageBuffer *work);
bool processor_dog(ImageProcessor *p, ImageBuffer *work, float sigma_small, float sigma_large, float gain);
bool processor_histogram_stretch(ImageProcessor *p, ImageBuffer *work);
bool processor_endpoint_detection(ImageProcessor *p, ImageBuffer *work, int threshold);
bool processor_median3x3(ImageProcessor *p, ImageBuffer *work);

bool processor_apply_cpu_effect(ImageProcessor *p, ImageBuffer *work, int alg_id,
                                const EffectParams *params);
const AlgorithmSpec *processor_algorithm_specs(int *count);
const char *processor_algorithm_name(int alg_id);

/* Static-mode reset: copy inImage -> outImage.
 * Reallocates outImage if its dimensions differ from inImage. */
bool processor_reset(AppState *state);

bool processor_can_append_effect(const AppState *state);

/* Call after gl_load_all + renderer_init (needs a current GL context). */
bool processor_init_gpu(ImageProcessor *p, char *err, size_t err_len);
void processor_destroy_gpu(ImageProcessor *p);

/* Apply GLSL effect for alg_id to state->outImage (static mode only).
   params may be NULL to use defaults.
   Returns false on error; on failure outImage may be partially modified. */
bool processor_apply_glsl_effect(ImageProcessor *p, AppState *state, int alg_id,
                                  const EffectParams *params, char *err, size_t err_len);
bool processor_apply_glsl_stack_to_texture(ImageProcessor *p, const ImageBuffer *src,
                                           const EffectCommand *stack, int count,
                                           GpuImage *result,
                                           char *err, size_t err_len);

#endif
