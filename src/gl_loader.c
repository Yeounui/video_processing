#include "gl_loader.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL_video.h>

PFNGLCREATESHADERPROC pglCreateShader = NULL;
PFNGLSHADERSOURCEPROC pglShaderSource = NULL;
PFNGLCOMPILESHADERPROC pglCompileShader = NULL;
PFNGLGETSHADERIVPROC pglGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC pglDeleteShader = NULL;
PFNGLCREATEPROGRAMPROC pglCreateProgram = NULL;
PFNGLATTACHSHADERPROC pglAttachShader = NULL;
PFNGLLINKPROGRAMPROC pglLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC pglGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = NULL;
PFNGLDELETEPROGRAMPROC pglDeleteProgram = NULL;
PFNGLUSEPROGRAMPROC pglUseProgram = NULL;
PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = NULL;
PFNGLUNIFORM1IPROC pglUniform1i = NULL;
PFNGLUNIFORM2FPROC pglUniform2f = NULL;
PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = NULL;
PFNGLGENBUFFERSPROC pglGenBuffers = NULL;
PFNGLBINDBUFFERPROC pglBindBuffer = NULL;
PFNGLBUFFERDATAPROC pglBufferData = NULL;
PFNGLDELETEBUFFERSPROC pglDeleteBuffers = NULL;
PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = NULL;
PFNGLACTIVETEXTUREPROC pglActiveTexture = NULL;
PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = NULL;
PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = NULL;
PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = NULL;
PFNGLUNIFORM1FPROC pglUniform1f = NULL;

static bool gl_missing(char *err_buf, size_t err_buf_len, const char *name)
{
    if (err_buf && err_buf_len > 0) {
        snprintf(err_buf, err_buf_len, "gl_load_all: missing %s", name);
    }
    return false;
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

bool gl_load_all(char *err_buf, size_t err_buf_len)
{
    if (err_buf && err_buf_len > 0) {
        err_buf[0] = '\0';
    }

    pglCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
    if (!pglCreateShader) {
        return gl_missing(err_buf, err_buf_len, "glCreateShader");
    }
    pglShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
    if (!pglShaderSource) {
        return gl_missing(err_buf, err_buf_len, "glShaderSource");
    }
    pglCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
    if (!pglCompileShader) {
        return gl_missing(err_buf, err_buf_len, "glCompileShader");
    }
    pglGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
    if (!pglGetShaderiv) {
        return gl_missing(err_buf, err_buf_len, "glGetShaderiv");
    }
    pglGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
    if (!pglGetShaderInfoLog) {
        return gl_missing(err_buf, err_buf_len, "glGetShaderInfoLog");
    }
    pglDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
    if (!pglDeleteShader) {
        return gl_missing(err_buf, err_buf_len, "glDeleteShader");
    }
    pglCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
    if (!pglCreateProgram) {
        return gl_missing(err_buf, err_buf_len, "glCreateProgram");
    }
    pglAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
    if (!pglAttachShader) {
        return gl_missing(err_buf, err_buf_len, "glAttachShader");
    }
    pglLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
    if (!pglLinkProgram) {
        return gl_missing(err_buf, err_buf_len, "glLinkProgram");
    }
    pglGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
    if (!pglGetProgramiv) {
        return gl_missing(err_buf, err_buf_len, "glGetProgramiv");
    }
    pglGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
    if (!pglGetProgramInfoLog) {
        return gl_missing(err_buf, err_buf_len, "glGetProgramInfoLog");
    }
    pglDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
    if (!pglDeleteProgram) {
        return gl_missing(err_buf, err_buf_len, "glDeleteProgram");
    }
    pglUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
    if (!pglUseProgram) {
        return gl_missing(err_buf, err_buf_len, "glUseProgram");
    }
    pglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
    if (!pglGetUniformLocation) {
        return gl_missing(err_buf, err_buf_len, "glGetUniformLocation");
    }
    pglUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
    if (!pglUniform1i) {
        return gl_missing(err_buf, err_buf_len, "glUniform1i");
    }
    pglUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
    if (!pglUniform2f) {
        return gl_missing(err_buf, err_buf_len, "glUniform2f");
    }
    pglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays");
    if (!pglGenVertexArrays) {
        return gl_missing(err_buf, err_buf_len, "glGenVertexArrays");
    }
    pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
    if (!pglBindVertexArray) {
        return gl_missing(err_buf, err_buf_len, "glBindVertexArray");
    }
    pglDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArrays");
    if (!pglDeleteVertexArrays) {
        return gl_missing(err_buf, err_buf_len, "glDeleteVertexArrays");
    }
    pglGenBuffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
    if (!pglGenBuffers) {
        return gl_missing(err_buf, err_buf_len, "glGenBuffers");
    }
    pglBindBuffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
    if (!pglBindBuffer) {
        return gl_missing(err_buf, err_buf_len, "glBindBuffer");
    }
    pglBufferData = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
    if (!pglBufferData) {
        return gl_missing(err_buf, err_buf_len, "glBufferData");
    }
    pglDeleteBuffers = (PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers");
    if (!pglDeleteBuffers) {
        return gl_missing(err_buf, err_buf_len, "glDeleteBuffers");
    }
    pglVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
    if (!pglVertexAttribPointer) {
        return gl_missing(err_buf, err_buf_len, "glVertexAttribPointer");
    }
    pglEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    if (!pglEnableVertexAttribArray) {
        return gl_missing(err_buf, err_buf_len, "glEnableVertexAttribArray");
    }
    pglActiveTexture = (PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture");
    if (!pglActiveTexture) {
        return gl_missing(err_buf, err_buf_len, "glActiveTexture");
    }
    pglGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
    if (!pglGenFramebuffers) {
        return gl_missing(err_buf, err_buf_len, "glGenFramebuffers");
    }
    pglDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers");
    if (!pglDeleteFramebuffers) {
        return gl_missing(err_buf, err_buf_len, "glDeleteFramebuffers");
    }
    pglBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    if (!pglBindFramebuffer) {
        return gl_missing(err_buf, err_buf_len, "glBindFramebuffer");
    }
    pglFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
    if (!pglFramebufferTexture2D) {
        return gl_missing(err_buf, err_buf_len, "glFramebufferTexture2D");
    }
    pglCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)SDL_GL_GetProcAddress("glCheckFramebufferStatus");
    if (!pglCheckFramebufferStatus) {
        return gl_missing(err_buf, err_buf_len, "glCheckFramebufferStatus");
    }
    pglUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f");
    if (!pglUniform1f) {
        return gl_missing(err_buf, err_buf_len, "glUniform1f");
    }

    return true;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void gl_check_error(const char *tag)
{
    GLenum e = glGetError();

    while (e != GL_NO_ERROR) {
        fprintf(stderr, "[GL error] %s: 0x%x\n", tag, (unsigned)e);
        e = glGetError();
    }
}
