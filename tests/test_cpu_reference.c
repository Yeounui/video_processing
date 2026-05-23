#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "image_processor.h"

static int g_fail = 0;

static void check_eq_bytes(const char *name, const uint8_t *got,
                            const uint8_t *want, size_t n) {
    if (memcmp(got, want, n) == 0) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
        for (size_t i = 0; i < n; ++i) {
            if (got[i] != want[i]) {
                printf("       byte %zu: got %u, want %u\n",
                       i, (unsigned)got[i], (unsigned)want[i]);
            }
        }
        g_fail++;
    }
}

static void check_true(const char *name, int cond) {
    if (cond) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
        g_fail++;
    }
}

/* --- Helpers --- */

static ImageBuffer make_buf(int w, int h, const uint8_t *values) {
    ImageBuffer b;
    b.width    = w;
    b.height   = h;
    b.channels = 3;
    size_t bytes = (size_t)w * (size_t)h * 3;
    b.data = malloc(bytes);
    /* values are single-channel; replicate to R=G=B */
    for (size_t i = 0; i < (size_t)(w * h); ++i) {
        b.data[i * 3 + 0] = values[i];
        b.data[i * 3 + 1] = values[i];
        b.data[i * 3 + 2] = values[i];
    }
    return b;
}

static ImageBuffer make_buf_rgb(int w, int h, const uint8_t *rgb_values) {
    ImageBuffer b;
    b.width    = w;
    b.height   = h;
    b.channels = 3;
    size_t bytes = (size_t)w * (size_t)h * 3;
    b.data = malloc(bytes);
    memcpy(b.data, rgb_values, bytes);
    return b;
}

static void free_buf(ImageBuffer *b) {
    free(b->data);
    b->data = NULL;
}

/* Expand per-pixel single-channel expected to 3-channel for comparison */
static void expand_rgb(const uint8_t *src, uint8_t *dst, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i * 3 + 0] = src[i];
        dst[i * 3 + 1] = src[i];
        dst[i * 3 + 2] = src[i];
    }
}

/* --- Tests --- */

static void test_brightness(ImageProcessor *p) {
    /* 3x3 grayscale (R=G=B) values 10..90 */
    static const uint8_t vals[9] = {10,20,30,40,50,60,70,80,90};

    /* +30 -> 40..120 */
    {
        static const uint8_t want1[9] = {40,50,60,70,80,90,100,110,120};
        uint8_t want_rgb[27];
        expand_rgb(want1, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_brightness(p, &b, 30);
        check_eq_bytes("brightness +30", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* -50 -> {0,0,0,0,0,10,20,30,40} (5 zeros due to clamping) */
    {
        /* 10-50=-40->0, 20-50=-30->0, 30-50=-20->0, 40-50=-10->0, 50-50=0,
           60-50=10, 70-50=20, 80-50=30, 90-50=40 */
        static const uint8_t want2[9] = {0,0,0,0,0,10,20,30,40};
        uint8_t want_rgb[27];
        expand_rgb(want2, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_brightness(p, &b, -50);
        check_eq_bytes("brightness -50 (clamped)", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* +250 -> all clamp to 255 */
    {
        uint8_t want_rgb[27];
        memset(want_rgb, 255, 27);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_brightness(p, &b, 250);
        check_eq_bytes("brightness +250 (clamp high)", b.data, want_rgb, 27);
        free_buf(&b);
    }
}

static void test_multiply(ImageProcessor *p) {
    /* factor 2.0: 10..90 -> 20..180 */
    {
        static const uint8_t vals[9] = {10,20,30,40,50,60,70,80,90};
        static const uint8_t want[9] = {20,40,60,80,100,120,140,160,180};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_multiply(p, &b, 2.0f);
        check_eq_bytes("multiply x2.0", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* factor 0.5: use {20,40,60,80} -> {10,20,30,40} (no half-cases) */
    {
        static const uint8_t vals[4] = {20,40,60,80};
        static const uint8_t want[4] = {10,20,30,40};
        uint8_t want_rgb[12], src_rgb[12];
        expand_rgb(want, want_rgb, 4);
        expand_rgb(vals, src_rgb, 4);
        ImageBuffer b = make_buf_rgb(2, 2, src_rgb);
        processor_multiply(p, &b, 0.5f);
        check_eq_bytes("multiply x0.5", b.data, want_rgb, 12);
        free_buf(&b);
    }

    /* factor 4.0 on {100,128,200} -> {255,255,255} (clamp) */
    {
        static const uint8_t vals[3] = {100,128,200};
        static const uint8_t want[3] = {255,255,255};
        uint8_t want_rgb[9];
        expand_rgb(want, want_rgb, 3);
        ImageBuffer b = make_buf(3, 1, vals);
        processor_multiply(p, &b, 4.0f);
        check_eq_bytes("multiply x4.0 (clamp)", b.data, want_rgb, 9);
        free_buf(&b);
    }
}

static void test_flip(ImageProcessor *p) {
    /*
     * 3x3 input (R=G=B):
     * 1 2 3
     * 4 5 6
     * 7 8 9
     */
    static const uint8_t grid[9] = {1,2,3,4,5,6,7,8,9};

    /* Horizontal: 3 2 1 / 6 5 4 / 9 8 7 */
    {
        static const uint8_t want[9] = {3,2,1,6,5,4,9,8,7};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, grid);
        processor_flip(p, &b, FLIP_HORIZONTAL);
        check_eq_bytes("flip horizontal", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* Vertical: 7 8 9 / 4 5 6 / 1 2 3 */
    {
        static const uint8_t want[9] = {7,8,9,4,5,6,1,2,3};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, grid);
        processor_flip(p, &b, FLIP_VERTICAL);
        check_eq_bytes("flip vertical", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* Both: 9 8 7 / 6 5 4 / 3 2 1 */
    {
        static const uint8_t want[9] = {9,8,7,6,5,4,3,2,1};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, grid);
        processor_flip(p, &b, FLIP_BOTH);
        check_eq_bytes("flip both", b.data, want_rgb, 27);
        free_buf(&b);
    }
}

static void test_rotate(ImageProcessor *p) {
    /*
     * 3x3 input (R=G=B):
     * 1 2 3
     * 4 5 6
     * 7 8 9
     *
     * Convention: positive degrees = counterclockwise (standard math).
     * Formula: theta = -degrees*pi/180; backward mapping from output to input.
     * cx=cy=1.0 for 3x3.
     */

    /* 0 degrees -> identity */
    {
        static const uint8_t grid[9] = {1,2,3,4,5,6,7,8,9};
        uint8_t want_rgb[27];
        expand_rgb(grid, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, grid);
        processor_rotate(p, &b, 0.0f);
        check_eq_bytes("rotate 0 (identity)", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* 90 degrees CCW:
     * Expected (verified by formula):
     * 7 4 1
     * 8 5 2
     * 9 6 3
     * Derivation: theta = -pi/2, cos=-0->0, sin=-1.
     * out(0,0): dx=-1,dy=-1 -> xi=0*(-1)-(-1)*(-1)+1=0-1+1=0? No:
     *   xi = cos(theta)*dx - sin(theta)*dy + cx
     *      = 0*(-1) - (-1)*(-1) + 1 = 0 - 1 + 1 = 0; yi = (-1)*(-1)+0*(-1)+1 = 1+1=2
     *   src(0,2)=7. Correct.
     */
    {
        static const uint8_t grid[9] = {1,2,3,4,5,6,7,8,9};
        static const uint8_t want[9] = {7,4,1,8,5,2,9,6,3};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, grid);
        processor_rotate(p, &b, 90.0f);
        check_eq_bytes("rotate 90 CCW", b.data, want_rgb, 27);
        free_buf(&b);
    }

    /* Expanded static-image rotate: non-square 90 degrees swaps canvas size. */
    {
        static const uint8_t grid[6] = {1,2,3,4,5,6};
        static const uint8_t want[6] = {5,3,1,6,4,2};
        uint8_t want_rgb[18];
        expand_rgb(want, want_rgb, 6);
        ImageBuffer b = make_buf(2, 3, grid);
        processor_rotate_expand(p, &b, 90.0f);
        check_true("rotate expand 90 dims", b.width == 3 && b.height == 2);
        check_eq_bytes("rotate expand 90 CCW", b.data, want_rgb, 18);
        free_buf(&b);
    }

    /* Expanded static-image rotate keeps the center and grows diagonal canvas. */
    {
        static const uint8_t grid[9] = {1,2,3,4,5,6,7,8,9};
        ImageBuffer b = make_buf(3, 3, grid);
        processor_rotate_expand(p, &b, 45.0f);
        check_true("rotate expand 45 dims", b.width == 5 && b.height == 5);
        check_true("rotate expand 45 center", b.data[((size_t)2 * 5u + 2u) * 3u] == 5);
        free_buf(&b);
    }

    /* Repeated static rotate trims unmapped black padding instead of compounding it. */
    {
        static const uint8_t grid[9] = {1,2,3,4,5,6,7,8,9};
        ImageBuffer b = make_buf(3, 3, grid);
        uint8_t *mask = NULL;
        int mask_w = 0;
        int mask_h = 0;
        processor_rotate_expand_masked(p, &b, &mask, &mask_w, &mask_h, 45.0f);
        processor_rotate_expand_masked(p, &b, &mask, &mask_w, &mask_h, 45.0f);
        check_true("rotate expand repeated stays tight", b.width <= 5 && b.height <= 5);
        free(mask);
        free_buf(&b);
    }
}

static void test_blur3x3(ImageProcessor *p) {
    /* 8x8 uniform (all 100) -> all 100 */
    {
        size_t n = 8 * 8;
        uint8_t *vals = malloc(n);
        memset(vals, 100, n);
        uint8_t *want_rgb = malloc(n * 3);
        uint8_t *want1    = malloc(n);
        memset(want1, 100, n);
        expand_rgb(want1, want_rgb, n);
        ImageBuffer b = make_buf(8, 8, vals);
        processor_blur3x3(p, &b);
        check_eq_bytes("blur3x3 uniform 100", b.data, want_rgb, n * 3);
        free_buf(&b);
        free(vals);
        free(want_rgb);
        free(want1);
    }

    /* 3x3 step image:
     * 0   0   0
     * 0   0   0
     * 255 255 255
     *
     * Expected output (1/9 uniform, clamp-to-edge):
     * row0: [0,   0,   0  ]  -- top neighborhood is all 0
     * row1: [85,  85,  85 ]  -- middle gets 3x255 from bottom row
     * row2: [170, 170, 170]  -- bottom clamped row adds extra 3x255
     *
     * Derivation for (0,2) bottom-left:
     *   y neighbors: clamp(-1,0,2)=[1,2,2], x neighbors: clamp(-1,0,1)=[0,0,1]
     *   Row y=1: img[1][0]=0, img[1][0]=0, img[1][1]=0  -> 0
     *   Row y=2: img[2][0]=255, img[2][0]=255, img[2][1]=255 -> 255*3
     *   Row y=2(clamped from y=3): same 3x255
     *   sum=1530, mean=round(1530/9)=170.
     */
    {
        static const uint8_t vals[9] = {0,0,0, 0,0,0, 255,255,255};
        static const uint8_t want[9] = {0,0,0, 85,85,85, 170,170,170};
        uint8_t want_rgb[27];
        /* want is already per-pixel single-channel expand */
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_blur3x3(p, &b);
        check_eq_bytes("blur3x3 step image", b.data, want_rgb, 27);
        free_buf(&b);
    }
}

static void test_new_point_filters(ImageProcessor *p) {
    {
        static const uint8_t vals[4] = {0, 64, 128, 255};
        uint8_t want_rgb[12];
        expand_rgb(vals, want_rgb, 4);
        ImageBuffer b = make_buf(2, 2, vals);
        processor_gamma(p, &b, 1.0f);
        check_eq_bytes("gamma 1.0 identity", b.data, want_rgb, 12);
        free_buf(&b);
    }
    {
        static const uint8_t vals[4] = {0, 126, 127, 255};
        static const uint8_t want[4] = {0, 0, 255, 255};
        uint8_t want_rgb[12];
        expand_rgb(want, want_rgb, 4);
        ImageBuffer b = make_buf(2, 2, vals);
        processor_threshold_fixed(p, &b, 127);
        check_eq_bytes("threshold fixed 127", b.data, want_rgb, 12);
        free_buf(&b);
    }
    {
        static const uint8_t vals[4] = {0, 64, 128, 255};
        static const uint8_t want[4] = {0, 0, 255, 255};
        uint8_t want_rgb[12];
        expand_rgb(want, want_rgb, 4);
        ImageBuffer b = make_buf(2, 2, vals);
        processor_threshold_average(p, &b);
        check_eq_bytes("threshold average", b.data, want_rgb, 12);
        free_buf(&b);
    }
    {
        static const uint8_t vals[3] = {0xff, 0xc9, 0x35};
        static const uint8_t want[3] = {0xc9, 0xc9, 0x01};
        uint8_t want_rgb[9];
        expand_rgb(want, want_rgb, 3);
        ImageBuffer b = make_buf(3, 1, vals);
        processor_bitwise_and(p, &b, 0xc9u);
        check_eq_bytes("bitwise and 0xc9", b.data, want_rgb, 9);
        free_buf(&b);
    }
    {
        static const uint8_t vals[3] = {10, 20, 30};
        static const uint8_t want[3] = {0, 128, 255};
        uint8_t want_rgb[9];
        expand_rgb(want, want_rgb, 3);
        ImageBuffer b = make_buf(3, 1, vals);
        processor_contrast_stretch(p, &b);
        check_eq_bytes("contrast stretch simple", b.data, want_rgb, 9);
        free_buf(&b);
    }
    {
        static const uint8_t rgb[9] = {30,60,90, 10,200,30, 255,0,0};
        static const uint8_t want[9] = {60,60,60, 80,80,80, 85,85,85};
        ImageBuffer b = make_buf_rgb(3, 1, rgb);
        processor_grayscale_average(p, &b);
        check_eq_bytes("grayscale average", b.data, want, 9);
        free_buf(&b);
    }
    {
        static const uint8_t rgb[9] = {30,60,90, 10,200,30, 255,0,0};
        static const uint8_t want[9] = {54,54,54, 124,124,124, 76,76,76};
        ImageBuffer b = make_buf_rgb(3, 1, rgb);
        processor_grayscale_luminosity(p, &b);
        check_eq_bytes("grayscale luminosity", b.data, want, 9);
        free_buf(&b);
    }
    {
        static const uint8_t rgb[9] = {30,60,90, 10,200,30, 255,0,0};
        static const uint8_t want[9] = {60,60,60, 105,105,105, 128,128,128};
        ImageBuffer b = make_buf_rgb(3, 1, rgb);
        processor_grayscale_lightness(p, &b);
        check_eq_bytes("grayscale lightness", b.data, want, 9);
        free_buf(&b);
    }
}

static void test_new_area_filters(ImageProcessor *p) {
    {
        static const uint8_t vals[9] = {50,50,50,50,50,50,50,50,50};
        uint8_t want_rgb[27];
        memset(want_rgb, 128, 27);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_emboss(p, &b);
        check_eq_bytes("emboss uniform -> 128", b.data, want_rgb, 27);
        free_buf(&b);
    }
    {
        static const uint8_t vals[25] = {
            100,100,100,100,100,
            100,100,100,100,100,
            100,100,100,100,100,
            100,100,100,100,100,
            100,100,100,100,100
        };
        uint8_t want_rgb[75];
        memset(want_rgb, 100, 75);
        ImageBuffer b = make_buf(5, 5, vals);
        processor_blur5x5(p, &b);
        check_eq_bytes("blur5x5 uniform 100", b.data, want_rgb, 75);
        free_buf(&b);
    }
    {
        static const uint8_t vals[9] = {0,0,0, 0,0,0, 255,255,255};
        static const uint8_t want[9] = {0,0,0, 255,255,255, 255,255,255};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_sobel_horizontal(p, &b, 1.0f);
        check_eq_bytes("sobel horizontal step", b.data, want_rgb, 27);
        free_buf(&b);
    }
    {
        static const uint8_t vals[9] = {0,0,255, 0,0,255, 0,0,255};
        static const uint8_t want[9] = {0,255,255, 0,255,255, 0,255,255};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_sobel_vertical(p, &b, 1.0f);
        check_eq_bytes("sobel vertical step", b.data, want_rgb, 27);
        free_buf(&b);
    }
    {
        static const uint8_t vals[9] = {0,0,0, 0,255,0, 0,0,0};
        static const uint8_t want[9] = {128,0,128, 0,255,0, 128,0,128};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_laplacian(p, &b);
        check_eq_bytes("laplacian impulse", b.data, want_rgb, 27);
        free_buf(&b);
    }
    {
        static const uint8_t vals[9] = {0,0,0, 255,255,255, 0,0,0};
        static const uint8_t want[9] = {0,0,0, 255,0,255, 0,0,0};
        uint8_t want_rgb[27];
        expand_rgb(want, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_endpoint_detection(p, &b, 127);
        check_eq_bytes("endpoint detection line ends", b.data, want_rgb, 27);
        free_buf(&b);
    }
    {
        static const uint8_t vals[9] = {1,2,3, 4,255,6, 7,8,9};
        static const uint8_t want_center[9] = {2,3,3, 4,6,6, 7,8,9};
        uint8_t want_rgb[27];
        expand_rgb(want_center, want_rgb, 9);
        ImageBuffer b = make_buf(3, 3, vals);
        processor_median3x3(p, &b);
        check_eq_bytes("median3x3 salt impulse", b.data, want_rgb, 27);
        free_buf(&b);
    }
}

static void test_algorithm_catalog_dispatch(ImageProcessor *p) {
    int count = 0;
    const AlgorithmSpec *specs = processor_algorithm_specs(&count);
    check_true("algorithm spec count", count == PROCESSOR_ALGORITHM_COUNT && specs != NULL);
    for (int i = 0; i < count; ++i)
        check_true("algorithm spec id sequence", specs[i].algorithmId == i + 1);

    for (int alg = 1; alg <= PROCESSOR_ALGORITHM_COUNT; ++alg) {
        static const uint8_t rgb[75] = {
             0,  0,  0,   20, 40, 60,   40, 80,120,   60,120,180,   80,160,240,
            10, 20, 30,   30, 60, 90,   50,100,150,   70,140,210,   90,180,255,
            20, 40, 60,   40, 80,120,   60,120,180,   80,160,240,  100,200,255,
            30, 60, 90,   50,100,150,   70,140,210,   90,180,255,  110,220,255,
            40, 80,120,   60,120,180,   80,160,240,  100,200,255,  120,240,255
        };
        EffectParams params = {{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
        ImageBuffer b = make_buf_rgb(5, 5, rgb);
        int before_w = b.width;
        int before_h = b.height;
        const char *name = processor_algorithm_name(alg);
        bool ok = processor_apply_cpu_effect(p, &b, alg, &params);
        check_true(name, ok && b.data != NULL &&
                         b.width == before_w && b.height == before_h &&
                         b.channels == 3);
        free_buf(&b);
    }
}

int main(void) {
    ImageProcessor p;
    processor_init(&p);

    test_brightness(&p);
    test_multiply(&p);
    test_flip(&p);
    test_rotate(&p);
    test_blur3x3(&p);
    test_new_point_filters(&p);
    test_new_area_filters(&p);
    test_algorithm_catalog_dispatch(&p);

    processor_destroy(&p);

    if (g_fail > 0) {
        printf("\n%d FAILED\n", g_fail);
        return 1;
    }
    printf("\nALL PASSED\n");
    return 0;
}
