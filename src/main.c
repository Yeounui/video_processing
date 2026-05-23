#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "gl_loader.h"
#include "renderer.h"
#include "types.h"
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "input.h"
#include "image_processor.h"
#include "event_handler.h"

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <image|video|/dev/videoN|rtsp-url>\n", prog);
    fprintf(stderr, "Open an image, video file, webcam device, or RTSP stream at startup.\n");
}

int main(int argc, char **argv) {
    char err[256];
    Renderer r;
    int i, j;

    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "No input source provided. Add an image, video, device, or stream path argument.\n");
        print_usage(argv[0]);
        return 1;
    }

    AppState state = {0};
    state.sourceType = SOURCE_NONE;
    state.running    = true;

    /* PPM self-test: runs before SDL to ensure output is visible even on SDL failure */
    {
        AppState selftest_state = {0};
        char selftest_err[256];
        FILE *ppm_f;
        bool selftest_ok = false;

        ppm_f = fopen("/tmp/vp_selftest.ppm", "wb");
        if (ppm_f) {
            /* Header: P6, 4x4, maxval 255, then one newline before binary data */
            fputs("P6\n4 4\n255\n", ppm_f);
            /* 48 bytes: 4 rows of 4 pixels each, 3 bytes per pixel
             * row 0: all red   (255,0,0)
             * row 1: all green (0,255,0)
             * row 2: all blue  (0,0,255)
             * row 3: all white (255,255,255) */
            static const unsigned char ppm_pixels[48] = {
                255,0,0,  255,0,0,  255,0,0,  255,0,0,  /* row 0: red   */
                0,255,0,  0,255,0,  0,255,0,  0,255,0,  /* row 1: green */
                0,0,255,  0,0,255,  0,0,255,  0,0,255,  /* row 2: blue  */
                255,255,255, 255,255,255, 255,255,255, 255,255,255 /* row 3: white */
            };
            fwrite(ppm_pixels, 1, 48, ppm_f);
            fclose(ppm_f);

            if (input_load_ppm(&selftest_state, "/tmp/vp_selftest.ppm", selftest_err, sizeof selftest_err)) {
                /* Check: pixel (0,0) = red (255,0,0) */
                /* Check: pixel at row 2, col 1 = index 9 in a 4-wide image = blue (0,0,255) */
                if (selftest_state.inImage.width  == 4 &&
                    selftest_state.inImage.height == 4 &&
                    selftest_state.inImage.channels == 3 &&
                    selftest_state.inImage.data[0] == 255 &&
                    selftest_state.inImage.data[1] == 0   &&
                    selftest_state.inImage.data[2] == 0   &&
                    selftest_state.inImage.data[9*3+0] == 0   &&
                    selftest_state.inImage.data[9*3+1] == 0   &&
                    selftest_state.inImage.data[9*3+2] == 255) {
                    printf("PPM self-test: PASS\n");
                    selftest_ok = true;
                } else {
                    fprintf(stderr, "PPM self-test: FAIL — pixel mismatch\n");
                }
            } else {
                fprintf(stderr, "PPM self-test: FAIL — %s\n", selftest_err);
            }

            input_close_source(&selftest_state);
            unlink("/tmp/vp_selftest.ppm");
        } else {
            fprintf(stderr, "PPM self-test: FAIL — cannot write /tmp/vp_selftest.ppm\n");
        }

        (void)selftest_ok; /* result is observational; continue regardless */
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = SDL_CreateWindow(
        "video_processing",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    if (!gl_load_all(err, sizeof err)) {
        fprintf(stderr, "gl_load_all failed: %s\n", err);
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!renderer_init(&r, window, err, sizeof err)) {
        fprintf(stderr, "renderer_init failed: %s\n", err);
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ImageProcessor proc = {0};
    if (!processor_init(&proc)) {
        fprintf(stderr, "processor_init failed\n");
        renderer_destroy(&r);
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    if (!processor_init_gpu(&proc, err, sizeof err)) {
        fprintf(stderr, "processor_init_gpu failed; using CPU fallback: %s\n", err);
        processor_destroy_gpu(&proc);
    }
    EventHandler events;
    events_init(&events, &state, &proc, &r);

    /* Show test pattern immediately so the window is visible before any slow load */
    state.outImage.width    = 16;
    state.outImage.height   = 16;
    state.outImage.channels = 3;
    state.outImage.data = (uint8_t *)malloc(16 * 16 * 3);
    if (!state.outImage.data) {
        fprintf(stderr, "malloc failed for test image\n");
        processor_destroy_gpu(&proc);
        processor_destroy(&proc);
        renderer_destroy(&r);
        SDL_GL_DeleteContext(glctx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    for (j = 0; j < 16; j++) {
        for (i = 0; i < 16; i++) {
            int block = (j / 4) * 4 + (i / 4);
            uint8_t *p = state.outImage.data + (j * 16 + i) * 3;
            switch (block % 4) {
                case 0: p[0]=255; p[1]=0;   p[2]=0;   break;
                case 1: p[0]=0;   p[1]=255; p[2]=0;   break;
                case 2: p[0]=0;   p[1]=0;   p[2]=255; break;
                case 3: p[0]=255; p[1]=255; p[2]=255; break;
            }
        }
    }
    renderer_upload_source_texture(&r, &state.outImage);
    renderer_resample(&r, 16, 16);
    renderer_render(&r);

    if (argc >= 2)
        events_open_path(&events, argv[1]);

    while (state.running) {
        events_process(&events);

        {
            bool got_frame = false;
            if (state.sourceType == SOURCE_VIDEO_FILE) {
                got_frame = input_read_next_frame(&state);
            } else if (state.sourceType == SOURCE_REALTIME_STREAM) {
                got_frame = input_read_realtime_frame(&state);
            }
            if (got_frame) {
                int k;
                bool used_gpu_stack = false;
                bool has_geom = false;
                size_t bc = (size_t)state.inImage.width *
                             (size_t)state.inImage.height * 3u;
                has_geom = state.imgFlipH || state.imgFlipV ||
                           state.imgRotateAngle < -0.001f || state.imgRotateAngle > 0.001f;
                if (!has_geom && state.effectCount > 0 && proc.gpuReady) {
                    GpuImage gpu_frame;
                    if (processor_apply_glsl_stack_to_texture(
                            &proc, &state.inImage, state.effectStack, state.effectCount,
                            &gpu_frame, err, sizeof err)) {
                        renderer_use_external_texture(&r, gpu_frame.textureId);
                        renderer_resample(&r, gpu_frame.width, gpu_frame.height);
                        used_gpu_stack = true;
                    } else {
                        fprintf(stderr, "GLSL stack failed: %s; falling back to CPU\n", err);
                    }
                }
                if (!used_gpu_stack) {
                    state.outImage.width = state.inImage.width;
                    state.outImage.height = state.inImage.height;
                    memcpy(state.outImage.data, state.inImage.data, bc);
                    for (k = 0; k < state.effectCount; k++) {
                        processor_apply_cpu_effect(&proc, &state.outImage,
                                                   state.effectStack[k].algorithmId,
                                                   &state.effectStack[k].params);
                    }
                    if (state.imgFlipH || state.imgFlipV) {
                        FlipMode fm = (state.imgFlipH && state.imgFlipV) ? FLIP_BOTH :
                                      state.imgFlipH ? FLIP_HORIZONTAL : FLIP_VERTICAL;
                        (void)processor_flip(&proc, &state.outImage, fm);
                    }
                    if (state.imgRotateAngle < -0.001f || state.imgRotateAngle > 0.001f) {
                        (void)processor_rotate_expand(&proc, &state.outImage, state.imgRotateAngle);
                    }
                    renderer_upload_source_texture(&r, &state.outImage);
                    renderer_resample(&r, state.outImage.width, state.outImage.height);
                }
            }
        }

        renderer_render(&r);
    }

    events_destroy(&events);
    processor_destroy_gpu(&proc);
    processor_destroy(&proc);
    input_close_source(&state);
    renderer_destroy(&r);
    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
