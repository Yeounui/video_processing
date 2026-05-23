#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "image_saver.h"

static void append_path_part(char *buf, size_t buflen, const char *part)
{
    size_t used;

    if (buflen == 0) {
        return;
    }

    used = strlen(buf);
    if (used >= buflen - 1) {
        return;
    }

    strncat(buf, part, buflen - used - 1);
}

static void make_timestamp_path(char *buf, size_t buflen, const char *ext)
{
    time_t now;
    struct tm *tm_info;
    char timestamp[32];
    char suffix_buf[16];
    int suffix;
    FILE *f;

    if (buflen == 0) {
        return;
    }

    now = time(NULL);
    tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
    if (buflen > 0) {
        buf[0] = '\0';
    }
    append_path_part(buf, buflen, "output_");
    append_path_part(buf, buflen, timestamp);
    append_path_part(buf, buflen, ext);

    suffix = 1;
    while ((f = fopen(buf, "rb")) != NULL) {
        fclose(f);
        snprintf(suffix_buf, sizeof(suffix_buf), "_%02d", suffix);
        if (buflen > 0) {
            buf[0] = '\0';
        }
        append_path_part(buf, buflen, "output_");
        append_path_part(buf, buflen, timestamp);
        append_path_part(buf, buflen, suffix_buf);
        append_path_part(buf, buflen, ext);
        suffix++;
    }
}

/* Returns the path to use: custom_path if provided, otherwise fills auto_buf
   with a timestamped name and returns that. */
static const char *resolve_path(const char *custom_path, char *auto_buf,
                                 size_t auto_buflen, const char *ext)
{
    if (custom_path != NULL && custom_path[0] != '\0') {
        return custom_path;
    }
    make_timestamp_path(auto_buf, auto_buflen, ext);
    return auto_buf;
}

bool saver_save_ppm(const ImageBuffer *img, const char *path, char *path_out, size_t path_len)
{
    char auto_path[256];
    const char *use_path;
    FILE *f;

    use_path = resolve_path(path, auto_path, sizeof(auto_path), ".ppm");
    if (path_out != NULL) {
        snprintf(path_out, path_len, "%s", use_path);
    }

    f = fopen(use_path, "wb");
    if (f == NULL) {
        fprintf(stderr, "saver_save_ppm: failed to write %s\n", use_path);
        return false;
    }

    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);
    fwrite(img->data, 1, (size_t)(img->width * img->height * 3), f);
    fclose(f);

    printf("Saved: %s\n", use_path);
    return true;
}

bool saver_save_png(const ImageBuffer *img, const char *path, char *path_out, size_t path_len)
{
    char auto_path[256];
    const char *use_path;
    int ok;

    use_path = resolve_path(path, auto_path, sizeof(auto_path), ".png");
    if (path_out != NULL) {
        snprintf(path_out, path_len, "%s", use_path);
    }

    ok = stbi_write_png(use_path, img->width, img->height, 3, img->data, img->width * 3);
    if (ok == 0) {
        fprintf(stderr, "saver_save_png: failed to write %s\n", use_path);
        return false;
    }

    printf("Saved: %s\n", use_path);
    return true;
}

bool saver_save_jpg(const ImageBuffer *img, const char *path, char *path_out, size_t path_len)
{
    char auto_path[256];
    const char *use_path;
    int ok;

    use_path = resolve_path(path, auto_path, sizeof(auto_path), ".jpg");
    if (path_out != NULL) {
        snprintf(path_out, path_len, "%s", use_path);
    }

    ok = stbi_write_jpg(use_path, img->width, img->height, 3, img->data, 90);
    if (ok == 0) {
        fprintf(stderr, "saver_save_jpg: failed to write %s\n", use_path);
        return false;
    }

    printf("Saved: %s\n", use_path);
    return true;
}

bool saver_save_bmp(const ImageBuffer *img, const char *path, char *path_out, size_t path_len)
{
    char auto_path[256];
    const char *use_path;
    int ok;

    use_path = resolve_path(path, auto_path, sizeof(auto_path), ".bmp");
    if (path_out != NULL) {
        snprintf(path_out, path_len, "%s", use_path);
    }

    ok = stbi_write_bmp(use_path, img->width, img->height, 3, img->data);
    if (ok == 0) {
        fprintf(stderr, "saver_save_bmp: failed to write %s\n", use_path);
        return false;
    }

    printf("Saved: %s\n", use_path);
    return true;
}

bool saver_save_tga(const ImageBuffer *img, const char *path, char *path_out, size_t path_len)
{
    char auto_path[256];
    const char *use_path;
    int ok;

    use_path = resolve_path(path, auto_path, sizeof(auto_path), ".tga");
    if (path_out != NULL) {
        snprintf(path_out, path_len, "%s", use_path);
    }

    ok = stbi_write_tga(use_path, img->width, img->height, 3, img->data);
    if (ok == 0) {
        fprintf(stderr, "saver_save_tga: failed to write %s\n", use_path);
        return false;
    }

    printf("Saved: %s\n", use_path);
    return true;
}
