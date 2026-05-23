#ifndef VIDEO_PROCESSING_INPUT_H
#define VIDEO_PROCESSING_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

/* Decode JPG/PNG/BMP/TGA/etc via stb_image (forced to 3-channel RGB).
 * Replaces inImage and copies bytes into outImage.
 * On success: previous inImage/outImage data are freed; new buffers owned by AppState.
 * On failure: AppState buffers are left unchanged; err filled. */
bool input_load_image(AppState *state, const char *path, char *err, size_t err_len);

/* Pure-C PPM P6 parser (binary). Supports maxval == 255 only.
 * Token handling: skip leading whitespace, '#' comments to end of line.
 * Same buffer ownership rules as input_load_image. */
bool input_load_ppm(AppState *state, const char *path, char *err, size_t err_len);

/* Free inImage and outImage buffers and zero their descriptors.
 * Safe to call on a zero-initialized AppState. Idempotent. */
void input_close_source(AppState *state);

/* Open a video file via FFmpeg. Sets sourceType=SOURCE_VIDEO_FILE, allocates
 * inImage+outImage, fills state->video (playing=false, loop=false, speed=1.0).
 * Closes any prior video source first. */
bool input_open_video(AppState *state, const char *path, char *err, size_t err_len);

/* PTS-driven decode: if playing and a frame is due, decode to inImage.
 * Returns true if inImage was updated; false if too early, paused, or EOF. */
bool input_read_next_frame(AppState *state);

/* Seek to fraction [0.0, 1.0] of total duration. Flushes decoder. */
void input_video_seek(AppState *state, float frac);

/* Seek near end then pause. */
void input_video_seek_end(AppState *state);

/* Reset wall/PTS timing reference (call after play-on or speed change). */
void input_video_reset_timing(void);

/* Decode exactly one frame into inImage regardless of timing (paused step). */
void input_video_step_frame(AppState *state);

/* Seek back one frame and decode it (paused step backward). */
void input_video_step_frame_backward(AppState *state);

/* Open a realtime stream (webcam via v4l2 or RTSP URL). Allocates inImage+outImage,
 * launches producer thread, fills state->realtime. */
bool input_open_realtime_stream(AppState *state, const char *url, char *err, size_t err_len);

/* Read next frame from realtime stream ring buffer into inImage if available.
 * Returns true if a frame was copied; false if buffer empty, paused, or disconnected. */
bool input_read_realtime_frame(AppState *state);

/* Reconnect to the previously opened realtime stream URL. */
void input_reconnect_realtime_stream(AppState *state);

/* Get realtime stream buffer status: frame count, buffer capacity, dropped frames. */
void input_realtime_status(int *count, int *cap, uint64_t *dropped);

/* Returns true if the file path has a recognised video extension. */
bool input_is_video_path(const char *path);

#endif /* VIDEO_PROCESSING_INPUT_H */
