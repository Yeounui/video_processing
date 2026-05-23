#ifndef VIDEO_PROCESSING_IMAGE_SAVER_H
#define VIDEO_PROCESSING_IMAGE_SAVER_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

/* path: if non-NULL and non-empty, save to that path; otherwise generate
   output_YYYYMMDD_HHMMSS.ext in the current directory.
   path_out: if non-NULL, receives the actual path used (up to path_len bytes).
   Returns true on success, false on error (prints reason to stderr).
   Caller must ensure sourceType == SOURCE_IMAGE before calling. */

bool saver_save_ppm(const ImageBuffer *img, const char *path, char *path_out, size_t path_len);
bool saver_save_png(const ImageBuffer *img, const char *path, char *path_out, size_t path_len);
bool saver_save_jpg(const ImageBuffer *img, const char *path, char *path_out, size_t path_len);
bool saver_save_bmp(const ImageBuffer *img, const char *path, char *path_out, size_t path_len);
bool saver_save_tga(const ImageBuffer *img, const char *path, char *path_out, size_t path_len);

#endif /* VIDEO_PROCESSING_IMAGE_SAVER_H */
