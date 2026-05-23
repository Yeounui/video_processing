#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "types.h"
#include "image_saver.h"

static void fail_ppm(const char *msg)
{
    fprintf(stderr, "FAIL PPM: %s\n", msg);
    exit(1);
}

static void fail_png(const char *msg)
{
    fprintf(stderr, "FAIL PNG: %s\n", msg);
    exit(1);
}

static unsigned char *read_entire_file(FILE *f, long *size_out)
{
    unsigned char *buf;
    long size;

    if (fseek(f, 0, SEEK_END) != 0) {
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        return NULL;
    }
    rewind(f);

    buf = malloc((size_t)size);
    if (buf == NULL) {
        return NULL;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        return NULL;
    }

    *size_out = size;
    return buf;
}

int main(void)
{
    ImageBuffer img;
    unsigned char *file_buf;
    FILE *f;
    long file_size;
    size_t i;
    int newline_count;
    size_t pixel_start;
    char ppm_path[128] = {0};
    char png_path[128] = {0};
    char png_path2[128] = {0};
    unsigned char png_sig[8];

    img.width = 2;
    img.height = 2;
    img.channels = 3;
    img.data = malloc(12);
    if (img.data == NULL) {
        fprintf(stderr, "FAIL PPM: malloc image data\n");
        exit(1);
    }

    {
        static const uint8_t pixels[12] = {
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            255, 255, 255
        };
        memcpy(img.data, pixels, sizeof(pixels));
    }

    if (!saver_save_ppm(&img, NULL, ppm_path, sizeof(ppm_path))) {
        fail_ppm("saver_save_ppm returned false");
    }

    f = fopen(ppm_path, "rb");
    if (f == NULL) {
        fail_ppm("could not open saved file");
    }

    file_buf = read_entire_file(f, &file_size);
    if (file_buf == NULL) {
        fclose(f);
        fail_ppm("could not read saved file");
    }

    if (file_size < 4) {
        free(file_buf);
        fclose(f);
        fail_ppm("file too small");
    }
    if (file_buf[0] != 0x50 || file_buf[1] != 0x36 || file_buf[2] != 0x0A) {
        free(file_buf);
        fclose(f);
        fail_ppm("missing P6 header prefix");
    }

    newline_count = 0;
    pixel_start = 0;
    for (i = 0; i < (size_t)file_size; ++i) {
        if (file_buf[i] == '\n') {
            newline_count++;
            if (newline_count == 3) {
                pixel_start = i + 1;
                break;
            }
        }
    }

    if (newline_count != 3 || pixel_start >= (size_t)file_size) {
        free(file_buf);
        fclose(f);
        fail_ppm("could not find pixel data");
    }
    if (file_buf[pixel_start] != 0xFF) {
        free(file_buf);
        fclose(f);
        fail_ppm("first pixel red channel is not 255");
    }

    free(file_buf);
    fclose(f);
    remove(ppm_path);

    if (!saver_save_png(&img, NULL, png_path, sizeof(png_path))) {
        fail_png("saver_save_png returned false");
    }

    f = fopen(png_path, "rb");
    if (f == NULL) {
        fail_png("could not open saved file");
    }

    if (fread(png_sig, 1, sizeof(png_sig), f) != sizeof(png_sig)) {
        fclose(f);
        fail_png("could not read PNG signature");
    }
    if (png_sig[0] != 0x89 || png_sig[1] != 0x50 ||
        png_sig[2] != 0x4E || png_sig[3] != 0x47) {
        fclose(f);
        fail_png("invalid PNG signature");
    }

    fclose(f);

    if (!saver_save_png(&img, NULL, png_path2, sizeof(png_path2))) {
        fail_png("second saver_save_png returned false");
    }
    if (strcmp(png_path, png_path2) == 0) {
        fail_png("auto filename collision");
    }

    f = fopen(png_path2, "rb");
    if (f == NULL) {
        fail_png("could not open second saved file");
    }
    fclose(f);

    remove(png_path);
    remove(png_path2);

    free(img.data);
    printf("ALL PASSED\n");
    return 0;
}
