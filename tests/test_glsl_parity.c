#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "gl_loader.h"
#include "image_processor.h"

static int g_fail = 0;

static void check_parity(const char *name, const uint8_t *got, const uint8_t *ref, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int diff = (int)got[i] - (int)ref[i];
        if (diff < -1 || diff > 1) {
            fprintf(stderr, "FAIL %s: idx=%d got=%d ref=%d diff=%d\n",
                    name, i, got[i], ref[i], diff);
            g_fail++;
            return;
        }
    }
    printf("PASS %s\n", name);
}

typedef struct {
    int alg_id;
    EffectParams params;
} ParityCase;

static EffectParams params_for_alg(int alg_id)
{
    EffectParams params;
    memset(&params, 0, sizeof params);
    switch (alg_id) {
    case 1:  params.values[0] = 30.0f; break;
    case 2:  params.values[0] = 1.2f; break;
    case 3:  params.values[0] = 2.2f; break;
    case 6:  params.values[0] = (float)0xC9u; break;
    case 7:  params.values[0] = 0.0f; break;
    case 8:  params.values[0] = 30.0f; break;
    case 13: params.values[0] = 5.0f; params.values[1] = 1.0f; break;
    case 14:
    case 15:
    case 16:
    case 19:
    case 20:
        params.values[0] = 1.0f;
        break;
    case 17:
    case 18:
        params.values[0] = 9.0f;
        break;
    case 22:
        params.values[0] = 1.0f;
        params.values[1] = 2.0f;
        params.values[2] = 1.0f;
        break;
    case 24:
        params.values[0] = 127.0f;
        break;
    default:
        break;
    }
    return params;
}

static bool make_cpu_reference(int alg_id, const EffectParams *params,
                               const uint8_t *input, int w, int h, uint8_t *ref)
{
    size_t bytes = (size_t)w * (size_t)h * 3;
    uint8_t *storage = malloc(bytes);
    if (!storage) return false;
    memcpy(storage, input, bytes);

    ImageProcessor cpu_proc;
    processor_init(&cpu_proc);
    ImageBuffer b;
    b.data = storage;
    b.width = w;
    b.height = h;
    b.channels = 3;

    bool ok = processor_apply_cpu_effect(&cpu_proc, &b, alg_id, params);
    if (ok) memcpy(ref, b.data, bytes);

    if (b.data != storage) free(b.data);
    processor_destroy(&cpu_proc);
    if (b.data == storage) free(storage);
    return ok;
}

static void run_parity_case(ImageProcessor *proc, AppState *state, int alg_id)
{
    const int w = state->inImage.width;
    const int h = state->inImage.height;
    size_t bytes = (size_t)w * (size_t)h * 3;
    EffectParams params = params_for_alg(alg_id);
    uint8_t *cpu_ref = malloc(bytes);
    char err[256];
    const char *name = processor_algorithm_name(alg_id);
    char label[128];

    if (!cpu_ref) {
        fprintf(stderr, "FAIL %s: malloc failed\n", name);
        g_fail++;
        return;
    }
    if (!make_cpu_reference(alg_id, &params, state->inImage.data, w, h, cpu_ref)) {
        fprintf(stderr, "FAIL %s CPU reference\n", name);
        free(cpu_ref);
        g_fail++;
        return;
    }

    memcpy(state->outImage.data, state->inImage.data, bytes);
    if (!processor_apply_glsl_effect(proc, state, alg_id, &params, err, sizeof err)) {
        fprintf(stderr, "FAIL %s GLSL: %s\n", name, err);
        free(cpu_ref);
        g_fail++;
        return;
    }

    snprintf(label, sizeof label, "alg %d %s", alg_id, name);
    check_parity(label, state->outImage.data, cpu_ref, (int)bytes);
    free(cpu_ref);
}

int main(void)
{
    /* 1. Create hidden SDL/GL window */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *win = SDL_CreateWindow("parity_test",
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED,
                                       64, 64,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    char gl_err[256];
    if (!gl_load_all(gl_err, sizeof gl_err)) {
        fprintf(stderr, "gl_load_all failed: %s\n", gl_err);
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* 2. Init processor */
    ImageProcessor proc;
    memset(&proc, 0, sizeof proc);
    processor_init(&proc);
    char err[256];
    if (!processor_init_gpu(&proc, err, sizeof err)) {
        fprintf(stderr, "processor_init_gpu failed: %s\n", err);
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* 3. Build 8x8 test image */
    AppState state;
    memset(&state, 0, sizeof state);
    state.sourceType = SOURCE_IMAGE;
    state.inImage.width    = 8;
    state.inImage.height   = 8;
    state.inImage.channels = 3;
    state.inImage.data     = malloc(8 * 8 * 3);
    state.outImage.width    = 8;
    state.outImage.height   = 8;
    state.outImage.channels = 3;
    state.outImage.data     = malloc(8 * 8 * 3);

    if (!state.inImage.data || !state.outImage.data) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    {
        int i;
        for (i = 0; i < 8 * 8 * 3; i++) {
            state.inImage.data[i] = (uint8_t)((i + (i / 3) * 7) % 192);
        }
    }

    {
        const int glsl_algs[] = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
            17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28
        };
        int i;
        for (i = 0; i < (int)(sizeof glsl_algs / sizeof glsl_algs[0]); ++i)
            run_parity_case(&proc, &state, glsl_algs[i]);
    }

    /* Teardown */
    processor_destroy_gpu(&proc);
    processor_destroy(&proc);
    free(state.inImage.data);
    free(state.outImage.data);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (g_fail == 0) {
        printf("ALL PASSED\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) FAILED\n", g_fail);
    return 1;
}
