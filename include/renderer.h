#ifndef VIDEO_PROCESSING_RENDERER_H
#define VIDEO_PROCESSING_RENDERER_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include "types.h"
#include "gl_loader.h"

typedef struct {
    SDL_Window  *window;        /* not owned — borrowed from main */
    int          windowW;
    int          windowH;

    GLuint       sourceTex;     /* GL_RGB8, GL_NEAREST */
    GLuint       displayTex;    /* currently rendered texture; defaults to sourceTex */
    int          sourceW;
    int          sourceH;

    GLuint       vao;
    GLuint       vbo;
    GLuint       program;

    GLint        uContentXY;    /* uniform vec2 u_contentXY  (in pixels) */
    GLint        uContentWH;    /* uniform vec2 u_contentWH  (in pixels) */
    GLint        uWindowSize;   /* uniform vec2 u_windowSize (in pixels) */
    GLint        uSampler;      /* uniform sampler2D u_source */
    GLint        uScale;        /* uniform float u_scale     (display scale factor) */
    GLint        uTexelSize;    /* uniform vec2 u_texelSize  (1/srcW, 1/srcH) */

    ContentRect  contentRect;   /* last computed; cached for AppState mirror */
} Renderer;

bool renderer_init(Renderer *r, SDL_Window *window, char *err, size_t err_len);
void renderer_resample(Renderer *r, int srcW, int srcH);
void renderer_upload_source_texture(Renderer *r, const ImageBuffer *img);
void renderer_use_external_texture(Renderer *r, unsigned int textureId);
void renderer_render(Renderer *r);
void renderer_destroy(Renderer *r);

#endif /* VIDEO_PROCESSING_RENDERER_H */
