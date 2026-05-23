#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "image_processor.h"
#include "gl_loader.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool processor_init(ImageProcessor *p) {
    p->scratch      = NULL;
    p->scratchBytes = 0;
    p->effectSrcTex = 0;
    p->effectDstTex = 0;
    p->effectFbo = 0;
    p->effectFboW = 0;
    p->effectFboH = 0;
    p->effectVao = 0;
    p->effectVbo = 0;
    for (int i = 0; i <= PROCESSOR_ALGORITHM_COUNT; ++i) p->effectPrograms[i] = 0;
    p->gpuReady = false;
    return true;
}

void processor_destroy(ImageProcessor *p) {
    free(p->scratch);
    p->scratch      = NULL;
    p->scratchBytes = 0;
}

void processor_clear_scratch(ImageProcessor *p) {
    free(p->scratch);
    p->scratch      = NULL;
    p->scratchBytes = 0;
}

static bool ensure_scratch(ImageProcessor *p, size_t needed) {
    if (p->scratchBytes >= needed) return true;
    uint8_t *grown = realloc(p->scratch, needed);
    if (!grown) return false;
    p->scratch      = grown;
    p->scratchBytes = needed;
    return true;
}

static void swap_work_with_scratch(ImageProcessor *p, ImageBuffer *work, size_t bytes) {
    uint8_t *tmp  = work->data;
    work->data    = p->scratch;
    p->scratch    = tmp;
    p->scratchBytes = bytes; /* always update: p->scratch now owns caller's alloc of exactly 'bytes' */
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float clamp_float(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t clamp_round_to_u8(float v) {
    int r = (int)roundf(v);
    return (uint8_t)clamp_int(r, 0, 255);
}

static size_t pix_off(int w, int x, int y) {
    return ((size_t)y * (size_t)w + (size_t)x) * 3;
}

static bool image_byte_count(int w, int h, size_t *bytes) {
    size_t pixels;
    if (w <= 0 || h <= 0) return false;
    if ((size_t)w > SIZE_MAX / (size_t)h) return false;
    pixels = (size_t)w * (size_t)h;
    if (pixels > SIZE_MAX / 3u) return false;
    *bytes = pixels * 3u;
    return true;
}

static bool mask_byte_count(int w, int h, size_t *bytes) {
    if (w <= 0 || h <= 0) return false;
    if ((size_t)w > SIZE_MAX / (size_t)h) return false;
    *bytes = (size_t)w * (size_t)h;
    return true;
}

static bool ensure_full_geometry_mask(uint8_t **mask, int *mask_w, int *mask_h, int w, int h) {
    uint8_t *new_mask;
    size_t bytes;

    if (*mask != NULL && *mask_w == w && *mask_h == h) return true;
    if (!mask_byte_count(w, h, &bytes)) return false;

    new_mask = (uint8_t *)malloc(bytes);
    if (!new_mask) return false;
    memset(new_mask, 1, bytes);

    free(*mask);
    *mask = new_mask;
    *mask_w = w;
    *mask_h = h;
    return true;
}

static bool crop_rgb_and_mask_to_bounds(uint8_t **rgb, uint8_t **mask, int *w, int *h) {
    int min_x = *w;
    int min_y = *h;
    int max_x = -1;
    int max_y = -1;
    uint8_t *src_rgb = *rgb;
    uint8_t *src_mask = *mask;
    uint8_t *cropped_rgb;
    uint8_t *cropped_mask;
    int crop_w;
    int crop_h;
    size_t crop_rgb_bytes;
    size_t crop_mask_bytes;

    for (int y = 0; y < *h; ++y) {
        for (int x = 0; x < *w; ++x) {
            if (src_mask[(size_t)y * (size_t)*w + (size_t)x] != 0) {
                if (x < min_x) min_x = x;
                if (y < min_y) min_y = y;
                if (x > max_x) max_x = x;
                if (y > max_y) max_y = y;
            }
        }
    }

    if (max_x < min_x || max_y < min_y) return true;
    if (min_x == 0 && min_y == 0 && max_x == *w - 1 && max_y == *h - 1) return true;

    crop_w = max_x - min_x + 1;
    crop_h = max_y - min_y + 1;
    if (!image_byte_count(crop_w, crop_h, &crop_rgb_bytes)) return false;
    if (!mask_byte_count(crop_w, crop_h, &crop_mask_bytes)) return false;

    cropped_rgb = (uint8_t *)malloc(crop_rgb_bytes);
    if (!cropped_rgb) return false;
    cropped_mask = (uint8_t *)malloc(crop_mask_bytes);
    if (!cropped_mask) {
        free(cropped_rgb);
        return false;
    }

    for (int y = 0; y < crop_h; ++y) {
        const uint8_t *rgb_row = src_rgb + pix_off(*w, min_x, min_y + y);
        const uint8_t *mask_row = src_mask + (size_t)(min_y + y) * (size_t)*w + (size_t)min_x;
        memcpy(cropped_rgb + (size_t)y * (size_t)crop_w * 3u,
               rgb_row,
               (size_t)crop_w * 3u);
        memcpy(cropped_mask + (size_t)y * (size_t)crop_w,
               mask_row,
               (size_t)crop_w);
    }

    free(src_rgb);
    free(src_mask);
    *rgb = cropped_rgb;
    *mask = cropped_mask;
    *w = crop_w;
    *h = crop_h;
    return true;
}

static uint8_t sample_channel(const uint8_t *src, int w, int h, int x, int y, int c) {
    x = clamp_int(x, 0, w - 1);
    y = clamp_int(y, 0, h - 1);
    return src[pix_off(w, x, y) + (size_t)c];
}

static float luminance_at(const uint8_t *src, int w, int h, int x, int y) {
    x = clamp_int(x, 0, w - 1);
    y = clamp_int(y, 0, h - 1);
    size_t off = pix_off(w, x, y);
    return 0.299f * (float)src[off + 0] +
           0.587f * (float)src[off + 1] +
           0.114f * (float)src[off + 2];
}

static int normalize_odd_distance(int distance) {
    distance = clamp_int(distance, 3, 31);
    if ((distance % 2) == 0) distance += (distance < 31) ? 1 : -1;
    return distance;
}

static int normalize_kernel_size(int kernel_size) {
    if (kernel_size <= 3) return 3;
    if (kernel_size >= 7) return 7;
    return 5;
}

static void build_gaussian_weights(int kernel_size, float sigma, float weights[7]) {
    int half = kernel_size / 2;
    float sum = 0.0f;
    sigma = clamp_float(sigma, 0.1f, 5.0f);
    for (int i = -half; i <= half; ++i) {
        float x = (float)i;
        float w = expf(-(x * x) / (2.0f * sigma * sigma));
        weights[i + half] = w;
        sum += w;
    }
    for (int i = 0; i < kernel_size; ++i) weights[i] /= sum;
}

static bool gaussian_to_buffer(const uint8_t *src, int w, int h,
                               int kernel_size, float sigma, uint8_t *dst) {
    kernel_size = normalize_kernel_size(kernel_size);
    int half = kernel_size / 2;
    float weights[7] = {0.0f};
    build_gaussian_weights(kernel_size, sigma, weights);

    size_t values = (size_t)w * (size_t)h * 3;
    float *tmp = malloc(values * sizeof(float));
    if (!tmp) return false;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c) {
                float acc = 0.0f;
                for (int k = -half; k <= half; ++k) {
                    acc += weights[k + half] *
                           (float)sample_channel(src, w, h, x + k, y, c);
                }
                tmp[off + (size_t)c] = acc;
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c) {
                float acc = 0.0f;
                for (int k = -half; k <= half; ++k) {
                    int sy = clamp_int(y + k, 0, h - 1);
                    acc += weights[k + half] *
                           tmp[pix_off(w, x, sy) + (size_t)c];
                }
                dst[off + (size_t)c] = clamp_round_to_u8(acc);
            }
        }
    }

    free(tmp);
    return true;
}

static bool unsharp_like(ImageProcessor *p, ImageBuffer *work, float strength) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    strength = clamp_float(strength, 0.0f, 3.0f);

    uint8_t *blur = malloc(bytes);
    if (!blur) return false;
    if (!gaussian_to_buffer(work->data, w, h, 5, 1.0f, blur)) {
        free(blur);
        return false;
    }
    if (!ensure_scratch(p, bytes)) {
        free(blur);
        return false;
    }

    for (size_t i = 0; i < bytes; ++i) {
        float src = (float)work->data[i];
        p->scratch[i] = clamp_round_to_u8(src + strength * (src - (float)blur[i]));
    }
    free(blur);
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_brightness(ImageProcessor *p, ImageBuffer *work, int delta) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    for (size_t i = 0; i < bytes; ++i) {
        int v = (int)src[i] + delta;
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        dst[i] = (uint8_t)v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_multiply(ImageProcessor *p, ImageBuffer *work, float factor) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    for (size_t i = 0; i < bytes; ++i) {
        float v = roundf((float)src[i] * factor);
        if (v < 0.0f)   v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        dst[i] = (uint8_t)v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_gamma(ImageProcessor *p, ImageBuffer *work, float gamma) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (gamma <= 0.0f) gamma = 1.0f;
    gamma = clamp_float(gamma, 0.1f, 5.0f);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    float inv_gamma = 1.0f / gamma;
    for (size_t i = 0; i < bytes; ++i) {
        float n = (float)src[i] / 255.0f;
        dst[i] = clamp_round_to_u8(255.0f * powf(n, inv_gamma));
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_threshold_fixed(ImageProcessor *p, ImageBuffer *work, int threshold) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    size_t bytes = pixels * 3;
    threshold = clamp_int(threshold, 0, 255);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3;
        float y = 0.299f * (float)src[off + 0] +
                  0.587f * (float)src[off + 1] +
                  0.114f * (float)src[off + 2];
        uint8_t v = (y >= (float)threshold) ? 255 : 0;
        dst[off + 0] = v;
        dst[off + 1] = v;
        dst[off + 2] = v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_threshold_average(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    double sum = 0.0;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3;
        sum += 0.299 * (double)work->data[off + 0] +
               0.587 * (double)work->data[off + 1] +
               0.114 * (double)work->data[off + 2];
    }
    return processor_threshold_fixed(p, work, (int)round(sum / (double)pixels));
}

bool processor_grayscale_average(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    size_t bytes = pixels * 3u;
    if (!ensure_scratch(p, bytes)) return false;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3u;
        float gray = ((float)work->data[off + 0] +
                      (float)work->data[off + 1] +
                      (float)work->data[off + 2]) / 3.0f;
        uint8_t v = clamp_round_to_u8(gray);
        p->scratch[off + 0] = v;
        p->scratch[off + 1] = v;
        p->scratch[off + 2] = v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_grayscale_luminosity(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    size_t bytes = pixels * 3u;
    if (!ensure_scratch(p, bytes)) return false;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3u;
        float gray = 0.299f * (float)work->data[off + 0] +
                     0.587f * (float)work->data[off + 1] +
                     0.114f * (float)work->data[off + 2];
        uint8_t v = clamp_round_to_u8(gray);
        p->scratch[off + 0] = v;
        p->scratch[off + 1] = v;
        p->scratch[off + 2] = v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_grayscale_lightness(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    size_t bytes = pixels * 3u;
    if (!ensure_scratch(p, bytes)) return false;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3u;
        int r = (int)work->data[off + 0];
        int g = (int)work->data[off + 1];
        int b = (int)work->data[off + 2];
        int lo = r < g ? r : g;
        int hi = r > g ? r : g;
        if (b < lo) lo = b;
        if (b > hi) hi = b;
        uint8_t v = clamp_round_to_u8(((float)lo + (float)hi) * 0.5f);
        p->scratch[off + 0] = v;
        p->scratch[off + 1] = v;
        p->scratch[off + 2] = v;
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_bitwise_and(ImageProcessor *p, ImageBuffer *work, unsigned int mask) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    uint8_t m = (uint8_t)(mask & 0xffu);
    if (!ensure_scratch(p, bytes)) return false;
    for (size_t i = 0; i < bytes; ++i) {
        p->scratch[i] = (uint8_t)(work->data[i] & m);
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_flip(ImageProcessor *p, ImageBuffer *work, FlipMode mode) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sx, sy;
            if (mode == FLIP_HORIZONTAL) {
                sx = w - 1 - x; sy = y;
            } else if (mode == FLIP_VERTICAL) {
                sx = x; sy = h - 1 - y;
            } else { /* FLIP_BOTH */
                sx = w - 1 - x; sy = h - 1 - y;
            }
            size_t d_off = ((size_t)y * (size_t)w + (size_t)x) * 3;
            size_t s_off = ((size_t)sy * (size_t)w + (size_t)sx) * 3;
            dst[d_off + 0] = src[s_off + 0];
            dst[d_off + 1] = src[s_off + 1];
            dst[d_off + 2] = src[s_off + 2];
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_rotate(ImageProcessor *p, ImageBuffer *work, float degrees) {
    /* Positive degrees = counterclockwise (standard math convention). */
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    float theta = -(degrees * (float)M_PI / 180.0f);
    float ct = cosf(theta);
    float st = sinf(theta);
    float cx = (float)(w - 1) / 2.0f;
    float cy = (float)(h - 1) / 2.0f;
    for (int yo = 0; yo < h; ++yo) {
        for (int xo = 0; xo < w; ++xo) {
            float dx = (float)xo - cx;
            float dy = (float)yo - cy;
            float xi = ct * dx - st * dy + cx;
            float yi = st * dx + ct * dy + cy;
            int xn = (int)roundf(xi);
            int yn = (int)roundf(yi);
            size_t d_off = ((size_t)yo * (size_t)w + (size_t)xo) * 3;
            if (xn < 0 || xn >= w || yn < 0 || yn >= h) {
                dst[d_off + 0] = 0;
                dst[d_off + 1] = 0;
                dst[d_off + 2] = 0;
            } else {
                size_t s_off = ((size_t)yn * (size_t)w + (size_t)xn) * 3;
                dst[d_off + 0] = src[s_off + 0];
                dst[d_off + 1] = src[s_off + 1];
                dst[d_off + 2] = src[s_off + 2];
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_rotate_expand(ImageProcessor *p, ImageBuffer *work, float degrees) {
    /* Static-image edit path: expand the canvas so rotated corners remain visible. */
    int w = work->width;
    int h = work->height;
    float theta = degrees * (float)M_PI / 180.0f;
    float ct = cosf(theta);
    float st = sinf(theta);
    int new_w;
    int new_h;
    size_t new_bytes;
    uint8_t *dst;
    const uint8_t *src = work->data;
    float ict;
    float ist;
    float src_cx = (float)(w - 1) / 2.0f;
    float src_cy = (float)(h - 1) / 2.0f;
    float dst_cx;
    float dst_cy;

    (void)p;
    if (fabsf(ct) < 1e-6f) ct = 0.0f;
    if (fabsf(st) < 1e-6f) st = 0.0f;
    ict = ct;
    ist = -st;
    new_w = (int)ceilf(fabsf((float)w * ct) + fabsf((float)h * st));
    new_h = (int)ceilf(fabsf((float)w * st) + fabsf((float)h * ct));
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    if (!image_byte_count(new_w, new_h, &new_bytes)) return false;

    dst = (uint8_t *)malloc(new_bytes);
    if (!dst) return false;

    dst_cx = (float)(new_w - 1) / 2.0f;
    dst_cy = (float)(new_h - 1) / 2.0f;
    for (int yo = 0; yo < new_h; ++yo) {
        for (int xo = 0; xo < new_w; ++xo) {
            float dx = (float)xo - dst_cx;
            float dy = (float)yo - dst_cy;
            float xi = ict * dx - ist * dy + src_cx;
            float yi = ist * dx + ict * dy + src_cy;
            int xn = (int)roundf(xi);
            int yn = (int)roundf(yi);
            size_t d_off = pix_off(new_w, xo, yo);
            if (xn < 0 || xn >= w || yn < 0 || yn >= h) {
                dst[d_off + 0] = 0;
                dst[d_off + 1] = 0;
                dst[d_off + 2] = 0;
            } else {
                size_t s_off = pix_off(w, xn, yn);
                dst[d_off + 0] = src[s_off + 0];
                dst[d_off + 1] = src[s_off + 1];
                dst[d_off + 2] = src[s_off + 2];
            }
        }
    }

    free(work->data);
    work->data = dst;
    work->width = new_w;
    work->height = new_h;
    work->channels = 3;
    return true;
}

bool processor_rotate_expand_masked(ImageProcessor *p, ImageBuffer *work,
                                    uint8_t **mask, int *mask_w, int *mask_h,
                                    float degrees) {
    int w = work->width;
    int h = work->height;
    float theta = degrees * (float)M_PI / 180.0f;
    float ct = cosf(theta);
    float st = sinf(theta);
    int new_w;
    int new_h;
    size_t new_bytes;
    size_t new_mask_bytes;
    uint8_t *dst;
    uint8_t *dst_mask;
    const uint8_t *src = work->data;
    const uint8_t *src_mask;
    float ict;
    float ist;
    float src_cx = (float)(w - 1) / 2.0f;
    float src_cy = (float)(h - 1) / 2.0f;
    float dst_cx;
    float dst_cy;

    (void)p;
    if (mask == NULL || mask_w == NULL || mask_h == NULL) {
        return processor_rotate_expand(p, work, degrees);
    }
    if (!ensure_full_geometry_mask(mask, mask_w, mask_h, w, h)) return false;
    src_mask = *mask;

    if (fabsf(ct) < 1e-6f) ct = 0.0f;
    if (fabsf(st) < 1e-6f) st = 0.0f;
    ict = ct;
    ist = -st;
    new_w = (int)ceilf(fabsf((float)w * ct) + fabsf((float)h * st));
    new_h = (int)ceilf(fabsf((float)w * st) + fabsf((float)h * ct));
    if (new_w < 1) new_w = 1;
    if (new_h < 1) new_h = 1;
    if (!image_byte_count(new_w, new_h, &new_bytes)) return false;
    if (!mask_byte_count(new_w, new_h, &new_mask_bytes)) return false;

    dst = (uint8_t *)malloc(new_bytes);
    if (!dst) return false;
    dst_mask = (uint8_t *)malloc(new_mask_bytes);
    if (!dst_mask) {
        free(dst);
        return false;
    }

    dst_cx = (float)(new_w - 1) / 2.0f;
    dst_cy = (float)(new_h - 1) / 2.0f;
    for (int yo = 0; yo < new_h; ++yo) {
        for (int xo = 0; xo < new_w; ++xo) {
            float dx = (float)xo - dst_cx;
            float dy = (float)yo - dst_cy;
            float xi = ict * dx - ist * dy + src_cx;
            float yi = ist * dx + ict * dy + src_cy;
            int xn = (int)roundf(xi);
            int yn = (int)roundf(yi);
            size_t d_off = pix_off(new_w, xo, yo);
            size_t m_off = (size_t)yo * (size_t)new_w + (size_t)xo;
            if (xn < 0 || xn >= w || yn < 0 || yn >= h ||
                src_mask[(size_t)yn * (size_t)w + (size_t)xn] == 0) {
                dst[d_off + 0] = 0;
                dst[d_off + 1] = 0;
                dst[d_off + 2] = 0;
                dst_mask[m_off] = 0;
            } else {
                size_t s_off = pix_off(w, xn, yn);
                dst[d_off + 0] = src[s_off + 0];
                dst[d_off + 1] = src[s_off + 1];
                dst[d_off + 2] = src[s_off + 2];
                dst_mask[m_off] = 1;
            }
        }
    }

    if (!crop_rgb_and_mask_to_bounds(&dst, &dst_mask, &new_w, &new_h)) {
        free(dst);
        free(dst_mask);
        return false;
    }

    free(work->data);
    free(*mask);
    work->data = dst;
    work->width = new_w;
    work->height = new_h;
    work->channels = 3;
    *mask = dst_mask;
    *mask_w = new_w;
    *mask_h = new_h;
    return true;
}

bool processor_blur3x3(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t       *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum[3] = {0, 0, 0};
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int sx = x + dx;
                    int sy = y + dy;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    if (sy < 0) sy = 0;
                    if (sy >= h) sy = h - 1;
                    size_t s_off = ((size_t)sy * (size_t)w + (size_t)sx) * 3;
                    sum[0] += src[s_off + 0];
                    sum[1] += src[s_off + 1];
                    sum[2] += src[s_off + 2];
                }
            }
            size_t d_off = ((size_t)y * (size_t)w + (size_t)x) * 3;
            for (int c = 0; c < 3; ++c) {
                float v = roundf((float)sum[c] / 9.0f);
                if (v < 0.0f)   v = 0.0f;
                if (v > 255.0f) v = 255.0f;
                dst[d_off + c] = (uint8_t)v;
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_emboss(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c) {
                float v = 128.0f
                        - (float)sample_channel(src, w, h, x - 1, y - 1, c)
                        + (float)sample_channel(src, w, h, x + 1, y + 1, c);
                dst[off + (size_t)c] = clamp_round_to_u8(v);
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_contrast_stretch(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    uint8_t low = 255;
    uint8_t high = 0;
    for (size_t i = 0; i < bytes; ++i) {
        if (work->data[i] < low) low = work->data[i];
        if (work->data[i] > high) high = work->data[i];
    }
    if (!ensure_scratch(p, bytes)) return false;
    if (high == low) {
        memcpy(p->scratch, work->data, bytes);
    } else {
        float scale = 255.0f / (float)(high - low);
        for (size_t i = 0; i < bytes; ++i) {
            p->scratch[i] = clamp_round_to_u8(((float)work->data[i] - (float)low) * scale);
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_blur5x5(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum[3] = {0, 0, 0};
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int c = 0; c < 3; ++c)
                        sum[c] += sample_channel(src, w, h, x + dx, y + dy, c);
                }
            }
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c)
                dst[off + (size_t)c] = clamp_round_to_u8((float)sum[c] / 25.0f);
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_gaussian_blur(ImageProcessor *p, ImageBuffer *work, int kernel_size, float sigma) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    if (!gaussian_to_buffer(work->data, w, h, kernel_size, sigma, p->scratch)) return false;
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_sharpen(ImageProcessor *p, ImageBuffer *work, float alpha) {
    return unsharp_like(p, work, alpha);
}

bool processor_high_pass_sharpen(ImageProcessor *p, ImageBuffer *work, float strength) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    strength = clamp_float(strength, 0.0f, 3.0f);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c) {
                float center = (float)sample_channel(src, w, h, x, y, c);
                float hval = 8.0f * center
                           - (float)sample_channel(src, w, h, x - 1, y - 1, c)
                           - (float)sample_channel(src, w, h, x,     y - 1, c)
                           - (float)sample_channel(src, w, h, x + 1, y - 1, c)
                           - (float)sample_channel(src, w, h, x - 1, y,     c)
                           - (float)sample_channel(src, w, h, x + 1, y,     c)
                           - (float)sample_channel(src, w, h, x - 1, y + 1, c)
                           - (float)sample_channel(src, w, h, x,     y + 1, c)
                           - (float)sample_channel(src, w, h, x + 1, y + 1, c);
                dst[off + (size_t)c] = clamp_round_to_u8(center + strength * hval);
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_high_boost(ImageProcessor *p, ImageBuffer *work, float beta) {
    return unsharp_like(p, work, beta);
}

bool processor_motion_blur_diagonal(ImageProcessor *p, ImageBuffer *work, int distance) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    distance = normalize_odd_distance(distance);
    int half = distance / 2;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum[3] = {0, 0, 0};
            for (int k = -half; k <= half; ++k) {
                for (int c = 0; c < 3; ++c)
                    sum[c] += sample_channel(src, w, h, x + k, y + k, c);
            }
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c)
                dst[off + (size_t)c] = clamp_round_to_u8((float)sum[c] / (float)distance);
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_motion_blur_horizontal(ImageProcessor *p, ImageBuffer *work, int distance) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    distance = normalize_odd_distance(distance);
    int half = distance / 2;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sum[3] = {0, 0, 0};
            for (int k = -half; k <= half; ++k) {
                for (int c = 0; c < 3; ++c)
                    sum[c] += sample_channel(src, w, h, x + k, y, c);
            }
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c)
                dst[off + (size_t)c] = clamp_round_to_u8((float)sum[c] / (float)distance);
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_sobel_horizontal(ImageProcessor *p, ImageBuffer *work, float scale) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    scale = clamp_float(scale, 0.1f, 4.0f);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float gy = -luminance_at(src, w, h, x - 1, y - 1)
                       -2.0f * luminance_at(src, w, h, x, y - 1)
                       -luminance_at(src, w, h, x + 1, y - 1)
                       +luminance_at(src, w, h, x - 1, y + 1)
                       +2.0f * luminance_at(src, w, h, x, y + 1)
                       +luminance_at(src, w, h, x + 1, y + 1);
            uint8_t v = clamp_round_to_u8(fabsf(gy) * scale);
            size_t off = pix_off(w, x, y);
            dst[off + 0] = v;
            dst[off + 1] = v;
            dst[off + 2] = v;
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_sobel_vertical(ImageProcessor *p, ImageBuffer *work, float scale) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    scale = clamp_float(scale, 0.1f, 4.0f);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float gx = -luminance_at(src, w, h, x - 1, y - 1)
                       +luminance_at(src, w, h, x + 1, y - 1)
                       -2.0f * luminance_at(src, w, h, x - 1, y)
                       +2.0f * luminance_at(src, w, h, x + 1, y)
                       -luminance_at(src, w, h, x - 1, y + 1)
                       +luminance_at(src, w, h, x + 1, y + 1);
            uint8_t v = clamp_round_to_u8(fabsf(gx) * scale);
            size_t off = pix_off(w, x, y);
            dst[off + 0] = v;
            dst[off + 1] = v;
            dst[off + 2] = v;
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_laplacian(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float l = 4.0f * luminance_at(src, w, h, x, y)
                    - luminance_at(src, w, h, x, y - 1)
                    - luminance_at(src, w, h, x - 1, y)
                    - luminance_at(src, w, h, x + 1, y)
                    - luminance_at(src, w, h, x, y + 1);
            uint8_t v = clamp_round_to_u8(l + 128.0f);
            size_t off = pix_off(w, x, y);
            dst[off + 0] = v;
            dst[off + 1] = v;
            dst[off + 2] = v;
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_dog(ImageProcessor *p, ImageBuffer *work, float sigma_small,
                   float sigma_large, float gain) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    sigma_small = clamp_float(sigma_small, 0.1f, 5.0f);
    sigma_large = clamp_float(sigma_large, 0.1f, 5.0f);
    if (sigma_large <= sigma_small) sigma_large = clamp_float(sigma_small + 0.1f, 0.1f, 5.0f);
    gain = clamp_float(gain, 0.1f, 4.0f);

    uint8_t *small = malloc(bytes);
    uint8_t *large = malloc(bytes);
    if (!small || !large) {
        free(small);
        free(large);
        return false;
    }
    if (!gaussian_to_buffer(work->data, w, h, 5, sigma_small, small) ||
        !gaussian_to_buffer(work->data, w, h, 5, sigma_large, large)) {
        free(small);
        free(large);
        return false;
    }
    if (!ensure_scratch(p, bytes)) {
        free(small);
        free(large);
        return false;
    }
    for (size_t i = 0; i < bytes; ++i)
        p->scratch[i] = clamp_round_to_u8(128.0f + gain * ((float)small[i] - (float)large[i]));
    free(small);
    free(large);
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_histogram_stretch(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t pixels = (size_t)w * (size_t)h;
    size_t bytes = pixels * 3;
    float low = 255.0f;
    float high = 0.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float yv = luminance_at(work->data, w, h, x, y);
            if (yv < low) low = yv;
            if (yv > high) high = yv;
        }
    }
    if (!ensure_scratch(p, bytes)) return false;
    if (high <= low) {
        memset(p->scratch, 0, bytes);
    } else {
        float scale = 255.0f / (high - low);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                uint8_t v = clamp_round_to_u8((luminance_at(work->data, w, h, x, y) - low) * scale);
                size_t off = pix_off(w, x, y);
                p->scratch[off + 0] = v;
                p->scratch[off + 1] = v;
                p->scratch[off + 2] = v;
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_endpoint_detection(ImageProcessor *p, ImageBuffer *work, int threshold) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    threshold = clamp_int(threshold, 0, 255);
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool fg = luminance_at(src, w, h, x, y) >= (float)threshold;
            int neighbors = 0;
            if (fg) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        int sx = x + dx;
                        int sy = y + dy;
                        if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
                        if (luminance_at(src, w, h, sx, sy) >= (float)threshold) neighbors++;
                    }
                }
            }
            uint8_t v = (fg && neighbors == 1) ? 255 : 0;
            size_t off = pix_off(w, x, y);
            dst[off + 0] = v;
            dst[off + 1] = v;
            dst[off + 2] = v;
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

bool processor_median3x3(ImageProcessor *p, ImageBuffer *work) {
    int w = work->width, h = work->height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (!ensure_scratch(p, bytes)) return false;
    const uint8_t *src = work->data;
    uint8_t *dst = p->scratch;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t off = pix_off(w, x, y);
            for (int c = 0; c < 3; ++c) {
                uint8_t vals[9];
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx)
                        vals[n++] = sample_channel(src, w, h, x + dx, y + dy, c);
                }
                for (int i = 1; i < 9; ++i) {
                    uint8_t key = vals[i];
                    int j = i - 1;
                    while (j >= 0 && vals[j] > key) {
                        vals[j + 1] = vals[j];
                        --j;
                    }
                    vals[j + 1] = key;
                }
                dst[off + (size_t)c] = vals[4];
            }
        }
    }
    swap_work_with_scratch(p, work, bytes);
    return true;
}

static const AlgorithmSpec kAlgorithmSpecs[PROCESSOR_ALGORITHM_COUNT] = {
    { 1,  BACKEND_GLSL,     1, false, true  },
    { 2,  BACKEND_GLSL,     1, false, true  },
    { 3,  BACKEND_GLSL,     1, false, true  },
    { 4,  BACKEND_GLSL,     1, false, true  },
    { 5,  BACKEND_CPU_GLSL, 1, true,  true  },
    { 6,  BACKEND_GLSL,     1, false, true  },
    { 7,  BACKEND_GLSL,     1, false, true  },
    { 8,  BACKEND_GLSL,     1, false, true  },
    { 9,  BACKEND_GLSL,     1, false, true  },
    { 10, BACKEND_CPU_GLSL, 1, true,  true  },
    { 11, BACKEND_GLSL,     1, false, true  },
    { 12, BACKEND_GLSL,     1, false, true  },
    { 13, BACKEND_GLSL,     2, false, true  },
    { 14, BACKEND_GLSL,     2, false, true  },
    { 15, BACKEND_GLSL,     1, false, true  },
    { 16, BACKEND_GLSL,     2, false, true  },
    { 17, BACKEND_GLSL,     1, false, true  },
    { 18, BACKEND_GLSL,     1, false, true  },
    { 19, BACKEND_GLSL,     1, false, true  },
    { 20, BACKEND_GLSL,     1, false, true  },
    { 21, BACKEND_GLSL,     1, false, true  },
    { 22, BACKEND_GLSL,     3, false, true  },
    { 23, BACKEND_CPU_GLSL, 1, true,  true  },
    { 24, BACKEND_GLSL,     1, false, true  },
    { 25, BACKEND_GLSL,     1, false, true  },
    { 26, BACKEND_GLSL,     1, false, true  },
    { 27, BACKEND_GLSL,     1, false, true  },
    { 28, BACKEND_GLSL,     1, false, true  }
};

static const char *kAlgorithmNames[PROCESSOR_ALGORITHM_COUNT] = {
    "Brightness",
    "Multiply",
    "Gamma Correction",
    "Threshold Fixed",
    "Threshold Average",
    "Bitwise AND",
    "Flip",
    "Rotation",
    "Emboss",
    "Contrast Stretching",
    "3x3 Blur",
    "5x5 Blur",
    "Gaussian Blur",
    "Sharpen",
    "High-Pass Sharpening",
    "Low-Pass-Removal Sharpening",
    "Motion Blur Diagonal",
    "Motion Blur Horizontal",
    "Horizontal Edge",
    "Vertical Edge",
    "Laplacian",
    "Difference of Gaussians",
    "Histogram Stretching",
    "Endpoint Detection",
    "Median Smoothing",
    "Grayscale Average",
    "Grayscale Luminosity",
    "Grayscale Lightness"
};

const AlgorithmSpec *processor_algorithm_specs(int *count) {
    if (count) *count = PROCESSOR_ALGORITHM_COUNT;
    return kAlgorithmSpecs;
}

const char *processor_algorithm_name(int alg_id) {
    if (alg_id < 1 || alg_id > PROCESSOR_ALGORITHM_COUNT) return "Unknown";
    return kAlgorithmNames[alg_id - 1];
}

bool processor_apply_cpu_effect(ImageProcessor *p, ImageBuffer *work, int alg_id,
                                const EffectParams *params) {
    const float *v = params ? params->values : NULL;
    switch (alg_id) {
    case 1:  return processor_brightness(p, work, v ? (int)roundf(v[0]) : 30);
    case 2:  return processor_multiply(p, work, v ? v[0] : 1.2f);
    case 3:  return processor_gamma(p, work, v ? v[0] : 2.2f);
    case 4:  return processor_threshold_fixed(p, work, v ? (int)roundf(v[0]) : 127);
    case 5:  return processor_threshold_average(p, work);
    case 6:  return processor_bitwise_and(p, work, v ? (unsigned int)roundf(v[0]) : 0xC9u);
    case 7: {
        int mode = v ? (int)roundf(v[0]) : (int)FLIP_HORIZONTAL;
        mode = clamp_int(mode, (int)FLIP_HORIZONTAL, (int)FLIP_BOTH);
        return processor_flip(p, work, (FlipMode)mode);
    }
    case 8:  return processor_rotate(p, work, v ? clamp_float(v[0], -360.0f, 360.0f) : 30.0f);
    case 9:  return processor_emboss(p, work);
    case 10: return processor_contrast_stretch(p, work);
    case 11: return processor_blur3x3(p, work);
    case 12: return processor_blur5x5(p, work);
    case 13: return processor_gaussian_blur(p, work,
                                            v ? (int)roundf(v[0]) : 5,
                                            v ? v[1] : 1.0f);
    case 14: return processor_sharpen(p, work, v ? v[0] : 1.0f);
    case 15: return processor_high_pass_sharpen(p, work, v ? v[0] : 1.0f);
    case 16: return processor_high_boost(p, work, v ? v[0] : 1.0f);
    case 17: return processor_motion_blur_diagonal(p, work, v ? (int)roundf(v[0]) : 9);
    case 18: return processor_motion_blur_horizontal(p, work, v ? (int)roundf(v[0]) : 9);
    case 19: return processor_sobel_horizontal(p, work, v ? v[0] : 1.0f);
    case 20: return processor_sobel_vertical(p, work, v ? v[0] : 1.0f);
    case 21: return processor_laplacian(p, work);
    case 22: return processor_dog(p, work,
                                  v ? v[0] : 1.0f,
                                  v ? v[1] : 2.0f,
                                  v ? v[2] : 1.0f);
    case 23: return processor_histogram_stretch(p, work);
    case 24: return processor_endpoint_detection(p, work, v ? (int)roundf(v[0]) : 127);
    case 25: return processor_median3x3(p, work);
    case 26: return processor_grayscale_average(p, work);
    case 27: return processor_grayscale_luminosity(p, work);
    case 28: return processor_grayscale_lightness(p, work);
    default: return false;
    }
}

bool processor_reset(AppState *state) {
    if (state->inImage.data == NULL) return true;
    int w = state->inImage.width;
    int h = state->inImage.height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    if (state->outImage.data == NULL ||
        state->outImage.width  != w  ||
        state->outImage.height != h) {
        free(state->outImage.data);
        state->outImage.data = malloc(bytes);
        if (!state->outImage.data) {
            state->outImage.width    = 0;
            state->outImage.height   = 0;
            state->outImage.channels = 0;
            return false;
        }
        state->outImage.width    = w;
        state->outImage.height   = h;
        state->outImage.channels = 3;
    }
    memcpy(state->outImage.data, state->inImage.data, bytes);
    free(state->preRotateImage.data);
    state->preRotateImage = (ImageBuffer){0};
    state->imgRotateAngle = 0.0f;
    state->imgFlipH       = false;
    state->imgFlipV       = false;
    if (!ensure_full_geometry_mask(&state->geometryMask,
                                   &state->geometryMaskW,
                                   &state->geometryMaskH,
                                   w, h)) {
        return false;
    }
    memset(state->geometryMask, 1, (size_t)w * (size_t)h);
    return true;
}

bool processor_can_append_effect(const AppState *state) {
    return state->effectCount < MAX_EFFECT_STACK;
}

/* ---------------------------------------------------------------------------
 * Phase 7: GPU effect infrastructure
 * ---------------------------------------------------------------------------*/

static GLuint load_shader_from_file(const char *path, GLenum type, char *err, size_t err_len)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return 0; }
    rewind(f);
    char *src = malloc((size_t)sz + 1);
    if (!src) { fclose(f); return 0; }
    size_t rd = fread(src, 1, (size_t)sz, f);
    fclose(f);
    src[rd] = '\0';
    GLuint sh = glCreateShader(type);
    const char *csrc = src;
    glShaderSource(sh, 1, &csrc, NULL);
    glCompileShader(sh);
    free(src);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof log, NULL, log);
        if (err && err_len > 0) snprintf(err, err_len, "shader compile %s: %s", path, log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint load_effect_program(const char *frag_name, char *err, size_t err_len)
{
    const char *vert_paths[2] = { "shaders/resampling.vert", "../shaders/resampling.vert" };
    GLuint vs = 0;
    int i;
    for (i = 0; i < 2; i++) {
        vs = load_shader_from_file(vert_paths[i], GL_VERTEX_SHADER, err, err_len);
        if (vs) break;
    }
    if (!vs) return 0;

    char frag_path[256];
    const char *frag_prefixes[2] = { "shaders/", "../shaders/" };
    GLuint fs = 0;
    for (i = 0; i < 2; i++) {
        snprintf(frag_path, sizeof frag_path, "%s%s", frag_prefixes[i], frag_name);
        fs = load_shader_from_file(frag_path, GL_FRAGMENT_SHADER, err, err_len);
        if (fs) break;
    }
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint link_ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        if (err && err_len > 0) snprintf(err, err_len, "program link %s: %s", frag_name, log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static bool load_catalog_programs(ImageProcessor *p, char *err, size_t err_len)
{
    GLuint catalog = load_effect_program("effect_catalog.frag", err, err_len);
    if (!catalog) return false;

    for (int alg_id = 1; alg_id <= PROCESSOR_ALGORITHM_COUNT; ++alg_id) {
        p->effectPrograms[alg_id] = catalog;
    }
    return true;
}

static void compute_average_threshold(const ImageBuffer *image, float *threshold)
{
    size_t pixels = (size_t)image->width * (size_t)image->height;
    double sum = 0.0;
    for (size_t i = 0; i < pixels; ++i) {
        size_t off = i * 3;
        sum += 0.299 * (double)image->data[off + 0] +
               0.587 * (double)image->data[off + 1] +
               0.114 * (double)image->data[off + 2];
    }
    *threshold = (float)round(sum / (double)pixels);
}

static void compute_byte_min_max(const ImageBuffer *image, float *low, float *high)
{
    size_t bytes = (size_t)image->width * (size_t)image->height * 3;
    uint8_t lo = 255;
    uint8_t hi = 0;
    for (size_t i = 0; i < bytes; ++i) {
        if (image->data[i] < lo) lo = image->data[i];
        if (image->data[i] > hi) hi = image->data[i];
    }
    *low = (float)lo;
    *high = (float)hi;
}

static void compute_luma_min_max(const ImageBuffer *image, float *low, float *high)
{
    int w = image->width;
    int h = image->height;
    float lo = 255.0f;
    float hi = 0.0f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float v = luminance_at(image->data, w, h, x, y);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    *low = lo;
    *high = hi;
}

static bool ensure_fbo_at_size(ImageProcessor *p, int w, int h)
{
    if (p->effectFboW == w && p->effectFboH == h) return true;

    glBindTexture(GL_TEXTURE_2D, p->effectSrcTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindTexture(GL_TEXTURE_2D, p->effectDstTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, p->effectFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p->effectDstTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) return false;

    p->effectFboW = w;
    p->effectFboH = h;
    return true;
}

static bool attach_effect_target(GLuint texture)
{
    GLenum status;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, texture, 0);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

static bool effect_needs_cpu_stats(int alg_id)
{
    return alg_id == 5 || alg_id == 10 || alg_id == 23;
}

static void resolve_glsl_effect_params(int alg_id, const EffectParams *params,
                                       const ImageBuffer *stats_image,
                                       float *v0, float *v1, float *v2)
{
    *v0 = params ? params->values[0] : 0.0f;
    *v1 = params ? params->values[1] : 0.0f;
    *v2 = params ? params->values[2] : 0.0f;
    if (!params) {
        switch (alg_id) {
        case 1:  *v0 = 30.0f; break;
        case 2:  *v0 = 1.2f; break;
        case 3:  *v0 = 2.2f; break;
        case 6:  *v0 = (float)0xC9u; break;
        case 8:  *v0 = 30.0f; break;
        case 13: *v0 = 5.0f; *v1 = 1.0f; break;
        case 14:
        case 15:
        case 16:
        case 19:
        case 20:
            *v0 = 1.0f;
            break;
        case 17:
        case 18:
            *v0 = 9.0f;
            break;
        case 22:
            *v0 = 1.0f; *v1 = 2.0f; *v2 = 1.0f;
            break;
        case 24:
            *v0 = 127.0f;
            break;
        default:
            break;
        }
    }
    if (stats_image) {
        if (alg_id == 5) {
            compute_average_threshold(stats_image, v0);
        } else if (alg_id == 10) {
            compute_byte_min_max(stats_image, v0, v1);
        } else if (alg_id == 23) {
            compute_luma_min_max(stats_image, v0, v1);
        }
    }
}

static void configure_effect_program(GLuint prog, GLuint srcTex, int alg_id,
                                     int w, int h, float v0, float v1, float v2)
{
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(glGetUniformLocation(prog, "u_source"), 0);
    glUniform1i(glGetUniformLocation(prog, "u_alg_id"), alg_id);
    glUniform2f(glGetUniformLocation(prog, "u_texSize"),
                1.0f / (float)w, 1.0f / (float)h);
    glUniform1f(glGetUniformLocation(prog, "u_param0"), v0);
    glUniform1f(glGetUniformLocation(prog, "u_param1"), v1);
    glUniform1f(glGetUniformLocation(prog, "u_param2"), v2);
}

bool processor_init_gpu(ImageProcessor *p, char *err, size_t err_len)
{
    int i;
    p->effectSrcTex = 0; p->effectDstTex = 0; p->effectFbo = 0;
    p->effectFboW = 0; p->effectFboH = 0;
    p->effectVao = 0; p->effectVbo = 0;
    for (i = 0; i <= PROCESSOR_ALGORITHM_COUNT; i++) p->effectPrograms[i] = 0;
    p->gpuReady = false;

    /* Full-screen quad: 4 vertices, stride 16 bytes (vec2 pos, vec2 uv).
     * UV y is flipped relative to the display quad so that readback row 0
     * (FBO bottom) maps to CPU image row 0 (top-left origin). */
    float quad[16] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f
    };
    glGenVertexArrays(1, &p->effectVao);
    glBindVertexArray(p->effectVao);
    glGenBuffers(1, &p->effectVbo);
    glBindBuffer(GL_ARRAY_BUFFER, p->effectVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void *)(uintptr_t)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Compile the shared shader catalog program. Global-stat algorithms (#5, #10,
     * #23) compute scalar parameters on CPU before this shader pass. */
    if (!load_catalog_programs(p, err, err_len)) {
        processor_destroy_gpu(p);
        return false;
    }

    /* effectSrcTex */
    glGenTextures(1, &p->effectSrcTex);
    glBindTexture(GL_TEXTURE_2D, p->effectSrcTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* effectDstTex */
    glGenTextures(1, &p->effectDstTex);
    glBindTexture(GL_TEXTURE_2D, p->effectDstTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* FBO (no attachment yet) */
    glGenFramebuffers(1, &p->effectFbo);

    p->gpuReady = true;
    return true;
}

void processor_destroy_gpu(ImageProcessor *p)
{
    int i;
    GLuint deletedPrograms[PROCESSOR_ALGORITHM_COUNT + 1];
    int deletedProgramCount = 0;
    if (p->effectFbo)    { glDeleteFramebuffers(1, &p->effectFbo);    p->effectFbo    = 0; }
    if (p->effectDstTex) { glDeleteTextures(1, &p->effectDstTex);     p->effectDstTex = 0; }
    if (p->effectSrcTex) { glDeleteTextures(1, &p->effectSrcTex);     p->effectSrcTex = 0; }
    if (p->effectVbo)    { glDeleteBuffers(1, &p->effectVbo);          p->effectVbo    = 0; }
    if (p->effectVao)    { glDeleteVertexArrays(1, &p->effectVao);     p->effectVao    = 0; }
    for (i = 0; i <= PROCESSOR_ALGORITHM_COUNT; i++) {
        if (p->effectPrograms[i]) {
            GLuint prog = p->effectPrograms[i];
            bool already_deleted = false;
            int j;
            for (j = 0; j < deletedProgramCount; ++j) {
                if (deletedPrograms[j] == prog) {
                    already_deleted = true;
                    break;
                }
            }
            if (!already_deleted) {
                glDeleteProgram(prog);
                deletedPrograms[deletedProgramCount++] = prog;
            }
            p->effectPrograms[i] = 0;
        }
    }
    p->effectFboW = 0;
    p->effectFboH = 0;
    p->gpuReady = false;
}

bool processor_apply_glsl_effect(ImageProcessor *p, AppState *state, int alg_id,
                                  const EffectParams *params, char *err, size_t err_len)
{
    if (!p->gpuReady || alg_id < 1 || alg_id > PROCESSOR_ALGORITHM_COUNT ||
        !p->effectPrograms[alg_id] || !state->outImage.data) {
        if (err && err_len > 0)
            snprintf(err, err_len,
                     "apply_glsl_effect: invalid args or no program for alg %d", alg_id);
        return false;
    }
    int w = state->outImage.width;
    int h = state->outImage.height;
    if (!ensure_fbo_at_size(p, w, h)) {
        if (err && err_len > 0) snprintf(err, err_len, "apply_glsl_effect: FBO incomplete");
        return false;
    }

    /* Upload outImage (RGB) to effectSrcTex (RGBA8) */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, p->effectSrcTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE,
                    state->outImage.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* Render into FBO */
    glBindFramebuffer(GL_FRAMEBUFFER, p->effectFbo);
    if (!attach_effect_target(p->effectDstTex)) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (err && err_len > 0)
            snprintf(err, err_len, "apply_glsl_effect: FBO target incomplete");
        return false;
    }
    glViewport(0, 0, w, h);

    GLuint prog = p->effectPrograms[alg_id];
    float v0, v1, v2;
    resolve_glsl_effect_params(alg_id, params, &state->outImage, &v0, &v1, &v2);
    configure_effect_program(prog, p->effectSrcTex, alg_id, w, h, v0, v1, v2);

    glBindVertexArray(p->effectVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    /* Readback rendered result (RGBA) into outImage (RGB) */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, state->outImage.data);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool processor_apply_glsl_stack_to_texture(ImageProcessor *p, const ImageBuffer *src,
                                           const EffectCommand *stack, int count,
                                           GpuImage *result,
                                           char *err, size_t err_len)
{
    GLuint srcTex;
    GLuint dstTex;
    int k;
    int w;
    int h;

    if (!p->gpuReady || !src || !src->data || !stack || count <= 0 || !result) {
        if (err && err_len > 0)
            snprintf(err, err_len, "apply_glsl_stack_to_texture: invalid args");
        return false;
    }
    w = src->width;
    h = src->height;
    if (!ensure_fbo_at_size(p, w, h)) {
        if (err && err_len > 0)
            snprintf(err, err_len, "apply_glsl_stack_to_texture: FBO incomplete");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, p->effectFbo);
    if (!attach_effect_target(p->effectDstTex)) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (err && err_len > 0)
            snprintf(err, err_len, "apply_glsl_stack_to_texture: FBO target incomplete");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, p->effectSrcTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE,
                    src->data);
    glBindTexture(GL_TEXTURE_2D, 0);

    srcTex = p->effectSrcTex;
    dstTex = p->effectDstTex;

    glBindFramebuffer(GL_FRAMEBUFFER, p->effectFbo);
    glViewport(0, 0, w, h);
    glBindVertexArray(p->effectVao);

    for (k = 0; k < count; k++) {
        const EffectCommand *cmd = &stack[k];
        int alg_id = cmd->algorithmId;
        GLuint tmp;
        GLuint prog;
        float v0, v1, v2;
        const ImageBuffer *stats_image = NULL;

        if (alg_id < 1 || alg_id > PROCESSOR_ALGORITHM_COUNT || !p->effectPrograms[alg_id]) {
            if (err && err_len > 0)
                snprintf(err, err_len,
                         "apply_glsl_stack_to_texture: no program for alg %d", alg_id);
            goto fail;
        }
        if (effect_needs_cpu_stats(alg_id)) {
            if (k != 0) {
                if (err && err_len > 0)
                    snprintf(err, err_len,
                             "apply_glsl_stack_to_texture: alg %d needs CPU stats after GPU pass",
                             alg_id);
                goto fail;
            }
            stats_image = src;
        }
        if (!attach_effect_target(dstTex)) {
            if (err && err_len > 0)
                snprintf(err, err_len,
                         "apply_glsl_stack_to_texture: FBO target incomplete");
            goto fail;
        }

        prog = p->effectPrograms[alg_id];
        resolve_glsl_effect_params(alg_id, &cmd->params, stats_image, &v0, &v1, &v2);
        configure_effect_program(prog, srcTex, alg_id, w, h, v0, v1, v2);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        tmp = srcTex;
        srcTex = dstTex;
        dstTex = tmp;
    }

    (void)attach_effect_target(p->effectDstTex);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    result->textureId = srcTex;
    result->fboId = 0;
    result->width = w;
    result->height = h;
    result->internalFormat = GL_RGBA8;
    result->isFresh = true;
    return true;

fail:
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
}
