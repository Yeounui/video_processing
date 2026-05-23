#include "renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static FILE *open_shader(const char *name)
{
    char path[256];
    int n;
    FILE *f;

    n = snprintf(path, sizeof(path), "shaders/%s", name);
    if (n >= 0 && (size_t)n < sizeof(path)) {
        f = fopen(path, "rb");
        if (f) {
            return f;
        }
    }

    n = snprintf(path, sizeof(path), "../shaders/%s", name);
    if (n >= 0 && (size_t)n < sizeof(path)) {
        f = fopen(path, "rb");
        if (f) {
            return f;
        }
    }

    return NULL;
}

static char *read_file(const char *name, char *err, size_t err_len)
{
    FILE *f;
    long size;
    size_t expected;
    size_t got;
    char *buf;

    f = open_shader(name);
    if (!f) {
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: unable to open shader '%s'", name);
        }
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: fseek failed for shader '%s'", name);
        }
        return NULL;
    }

    size = ftell(f);
    if (size < 0) {
        fclose(f);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: ftell failed for shader '%s'", name);
        }
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: rewind failed for shader '%s'", name);
        }
        return NULL;
    }

    expected = (size_t)size;
    buf = (char *)malloc(expected + 1);
    if (!buf) {
        fclose(f);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: out of memory reading shader '%s'", name);
        }
        return NULL;
    }

    got = fread(buf, 1, expected, f);
    if (got != expected) {
        free(buf);
        fclose(f);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: fread failed for shader '%s'", name);
        }
        return NULL;
    }

    if (fclose(f) != 0) {
        free(buf);
        if (err && err_len > 0) {
            snprintf(err, err_len, "read_file: fclose failed for shader '%s'", name);
        }
        return NULL;
    }

    buf[expected] = '\0';
    return buf;
}

static GLuint compile_shader(GLenum type, const char *src, char *err, size_t err_len)
{
    GLuint shader;
    GLint ok;
    const GLchar *shader_src;

    shader = glCreateShader(type);
    if (!shader) {
        if (err && err_len > 0) {
            snprintf(err, err_len, "compile_shader: glCreateShader failed");
        }
        return 0;
    }

    shader_src = (const GLchar *)src;
    glShaderSource(shader, 1, &shader_src, NULL);
    glCompileShader(shader);

    ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (err && err_len > 0) {
            glGetShaderInfoLog(shader, (GLsizei)err_len, NULL, err);
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_program(GLuint vs, GLuint fs, char *err, size_t err_len)
{
    GLuint program;
    GLint ok;

    program = glCreateProgram();
    if (!program) {
        if (err && err_len > 0) {
            snprintf(err, err_len, "link_program: glCreateProgram failed");
        }
        return 0;
    }

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (err && err_len > 0) {
            glGetProgramInfoLog(program, (GLsizei)err_len, NULL, err);
        }
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

bool renderer_init(Renderer *r, SDL_Window *window, char *err, size_t err_len)
{
    char *vs_src;
    char *fs_src;
    GLuint vs;
    GLuint fs;
    float verts[16];

    memset(r, 0, sizeof(*r));
    if (err && err_len > 0) {
        err[0] = '\0';
    }

    r->window = window;
    SDL_GetWindowSize(window, &r->windowW, &r->windowH);

    vs_src = read_file("resampling.vert", err, err_len);
    if (!vs_src) {
        return false;
    }

    vs = compile_shader(GL_VERTEX_SHADER, vs_src, err, err_len);
    if (!vs) {
        free(vs_src);
        return false;
    }

    fs_src = read_file("resampling.frag", err, err_len);
    if (!fs_src) {
        glDeleteShader(vs);
        free(vs_src);
        return false;
    }

    fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, err, err_len);
    if (!fs) {
        glDeleteShader(vs);
        free(fs_src);
        free(vs_src);
        return false;
    }

    r->program = link_program(vs, fs, err, err_len);
    glDeleteShader(vs);
    glDeleteShader(fs);
    free(fs_src);
    free(vs_src);
    if (!r->program) {
        return false;
    }

    r->uContentXY  = glGetUniformLocation(r->program, "u_contentXY");
    r->uContentWH  = glGetUniformLocation(r->program, "u_contentWH");
    r->uWindowSize = glGetUniformLocation(r->program, "u_windowSize");
    r->uSampler    = glGetUniformLocation(r->program, "u_source");
    r->uScale      = glGetUniformLocation(r->program, "u_scale");
    r->uTexelSize  = glGetUniformLocation(r->program, "u_texelSize");

    verts[0] = -1.0f;
    verts[1] = 1.0f;
    verts[2] = 0.0f;
    verts[3] = 0.0f;
    verts[4] = 1.0f;
    verts[5] = 1.0f;
    verts[6] = 1.0f;
    verts[7] = 0.0f;
    verts[8] = -1.0f;
    verts[9] = -1.0f;
    verts[10] = 0.0f;
    verts[11] = 1.0f;
    verts[12] = 1.0f;
    verts[13] = -1.0f;
    verts[14] = 1.0f;
    verts[15] = 1.0f;

    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);
    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)(sizeof(float)*2));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenTextures(1, &r->sourceTex);
    glBindTexture(GL_TEXTURE_2D, r->sourceTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    r->displayTex = r->sourceTex;
    r->sourceW = 0;
    r->sourceH = 0;
    return true;
}

void renderer_resample(Renderer *r, int srcW, int srcH)
{
    float scaleX;
    float scaleY;
    float scale;
    int cw;
    int ch;
    int cx;
    int cy;

    scaleX = (float)r->windowW / (float)srcW;
    scaleY = (float)r->windowH / (float)srcH;
    scale = scaleX < scaleY ? scaleX : scaleY;
    cw = (int)floorf((float)srcW * scale);
    ch = (int)floorf((float)srcH * scale);
    cx = (r->windowW - cw) / 2;
    cy = (r->windowH - ch) / 2;

    r->contentRect.x = cx;
    r->contentRect.y = cy;
    r->contentRect.w = cw;
    r->contentRect.h = ch;

    glUseProgram(r->program);
    glUniform2f(r->uContentXY,  (float)cx, (float)cy);
    glUniform2f(r->uContentWH,  (float)cw, (float)ch);
    glUniform2f(r->uWindowSize, (float)r->windowW, (float)r->windowH);
    glUniform1f(r->uScale,      scale);
    glUniform2f(r->uTexelSize,  1.0f / (float)srcW, 1.0f / (float)srcH);
    glUseProgram(0);
}

void renderer_upload_source_texture(Renderer *r, const ImageBuffer *img)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  /* CRITICAL for RGB 3-byte rows */
    glBindTexture(GL_TEXTURE_2D, r->sourceTex);

    if (img->width != r->sourceW || img->height != r->sourceH) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, img->width, img->height, 0, GL_RGB, GL_UNSIGNED_BYTE, img->data);
        r->sourceW = img->width;
        r->sourceH = img->height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_RGB, GL_UNSIGNED_BYTE, img->data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    r->displayTex = r->sourceTex;
}

void renderer_use_external_texture(Renderer *r, unsigned int textureId)
{
    r->displayTex = (GLuint)textureId;
}

void renderer_render(Renderer *r)
{
    GLuint tex = r->displayTex ? r->displayTex : r->sourceTex;

    glViewport(0, 0, r->windowW, r->windowH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(r->program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(r->uSampler, 0);
    glBindVertexArray(r->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    gl_check_error("renderer_render");
    SDL_GL_SwapWindow(r->window);
}

void renderer_destroy(Renderer *r)
{
    if (r->sourceTex) {
        glDeleteTextures(1, &r->sourceTex);
    }
    if (r->vbo) {
        glDeleteBuffers(1, &r->vbo);
    }
    if (r->vao) {
        glDeleteVertexArrays(1, &r->vao);
    }
    if (r->program) {
        glDeleteProgram(r->program);
    }

    memset(r, 0, sizeof(*r));
}
