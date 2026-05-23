#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <SDL2/SDL.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "input.h"

typedef struct {
    AVFormatContext *fmt_ctx;
    AVCodecContext  *codec_ctx;
    SwsContext      *sws_ctx;
    AVFrame         *frame;
    AVPacket        *packet;
    int              video_stream_idx;
    double           time_base_s;
    double           frame_dur_s;
    double           wall_ref_s;   /* 0.0 = not initialized */
    double           pts_ref_s;
} VideoDecoder;

static VideoDecoder g_vid = {0};

#define RT_BUF_COUNT 3

typedef struct {
    AVFormatContext *fmt_ctx;
    AVCodecContext  *codec_ctx;
    SwsContext      *sws_ctx;
    AVFrame         *frame;
    AVPacket        *packet;
    int              video_stream_idx;
    uint8_t         *slots[RT_BUF_COUNT];
    int              slot_bytes;
    int              write_idx;
    int              count;
    uint64_t         dropped;
    SDL_mutex       *mutex;
    SDL_cond        *cond;
    SDL_Thread      *thread;
    volatile int     stop_flag;
    volatile int     disconnected;
    char             url[512];
    int              width;
    int              height;
    float            fps;
} RealtimeDecoder;

static RealtimeDecoder g_rt = {0};

static double get_wall_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int skip_ws_and_comments(FILE *f)
{
    int c;

    while ((c = fgetc(f)) != EOF) {
        if (isspace(c)) {
            continue;
        }
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n') {
            }
            continue;
        }
        return c;
    }

    return EOF;
}

static bool read_ppm_token(FILE *f, char *buf, int buflen)
{
    int c;
    int i;

    if (buflen <= 1) {
        return false;
    }

    c = skip_ws_and_comments(f);
    if (c == EOF) {
        return false;
    }

    buf[0] = (char)c;
    i = 1;

    while (true) {
        int d = fgetc(f);

        if (d == EOF) {
            break;
        }
        if (isspace(d)) {
            if (ungetc(d, f) == EOF) {
                return false;
            }
            break;
        }
        if (i >= buflen - 1) {
            return false;
        }
        buf[i] = (char)d;
        i++;
    }

    buf[i] = '\0';
    return true;
}

bool input_load_image(AppState *state, const char *path, char *err, size_t err_len)
{
    int w = 0;
    int h = 0;
    int channels_in_file = 0;
    size_t byte_count;
    unsigned char *stbi_ptr;
    uint8_t *buf;
    uint8_t *out_buf;

    stbi_ptr = stbi_load(path, &w, &h, &channels_in_file, 3);
    if (stbi_ptr == NULL) {
        snprintf(err, err_len, "input_load_image: %s — %s", path, stbi_failure_reason());
        return false;
    }

    byte_count = (size_t)w * (size_t)h * 3u;
    buf = (uint8_t *)malloc(byte_count);
    if (buf == NULL) {
        stbi_image_free(stbi_ptr);
        snprintf(err, err_len, "input_load_image: out of memory");
        return false;
    }

    memcpy(buf, stbi_ptr, byte_count);
    stbi_image_free(stbi_ptr);

    out_buf = (uint8_t *)malloc(byte_count);
    if (out_buf == NULL) {
        free(buf);
        snprintf(err, err_len, "input_load_image: out of memory");
        return false;
    }

    memcpy(out_buf, buf, byte_count);

    free(state->inImage.data);
    free(state->outImage.data);
    free(state->geometryMask);

    state->inImage = (ImageBuffer){.data = buf, .width = w, .height = h, .channels = 3};
    state->outImage = (ImageBuffer){.data = out_buf, .width = w, .height = h, .channels = 3};
    state->geometryMask = NULL;
    state->geometryMaskW = 0;
    state->geometryMaskH = 0;

    return true;
}

bool input_load_ppm(AppState *state, const char *path, char *err, size_t err_len)
{
    FILE *f;
    char token[64];
    int w;
    int h;
    int maxval;
    size_t byte_count;
    size_t bytes_read;
    uint8_t *buf;
    uint8_t *out_buf;

    f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(err, err_len, "input_load_ppm: cannot open '%s'", path);
        return false;
    }

    if (!read_ppm_token(f, token, (int)sizeof(token)) || strcmp(token, "P6") != 0) {
        snprintf(err, err_len, "input_load_ppm: not a P6 PPM");
        fclose(f);
        return false;
    }

    if (!read_ppm_token(f, token, (int)sizeof(token))) {
        snprintf(err, err_len, "input_load_ppm: invalid dimensions 0x0");
        fclose(f);
        return false;
    }
    w = atoi(token);

    if (!read_ppm_token(f, token, (int)sizeof(token))) {
        snprintf(err, err_len, "input_load_ppm: invalid dimensions %dx0", w);
        fclose(f);
        return false;
    }
    h = atoi(token);

    if (!read_ppm_token(f, token, (int)sizeof(token))) {
        snprintf(err, err_len, "input_load_ppm: only maxval=255 supported, got 0");
        fclose(f);
        return false;
    }
    maxval = atoi(token);

    if (maxval != 255) {
        snprintf(err, err_len, "input_load_ppm: only maxval=255 supported, got %d", maxval);
        fclose(f);
        return false;
    }

    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) {
        snprintf(err, err_len, "input_load_ppm: invalid dimensions %dx%d", w, h);
        fclose(f);
        return false;
    }

    (void)fgetc(f);

    byte_count = (size_t)w * (size_t)h * 3u;
    buf = (uint8_t *)malloc(byte_count);
    if (buf == NULL) {
        snprintf(err, err_len, "input_load_ppm: out of memory");
        fclose(f);
        return false;
    }

    bytes_read = fread(buf, 1u, byte_count, f);
    if (bytes_read != byte_count) {
        snprintf(err, err_len, "input_load_ppm: truncated pixel data (read %zu of %zu)", bytes_read, byte_count);
        free(buf);
        fclose(f);
        return false;
    }

    out_buf = (uint8_t *)malloc(byte_count);
    if (out_buf == NULL) {
        free(buf);
        snprintf(err, err_len, "input_load_ppm: out of memory");
        fclose(f);
        return false;
    }

    memcpy(out_buf, buf, byte_count);

    free(state->inImage.data);
    free(state->outImage.data);
    free(state->geometryMask);

    state->inImage = (ImageBuffer){.data = buf, .width = w, .height = h, .channels = 3};
    state->outImage = (ImageBuffer){.data = out_buf, .width = w, .height = h, .channels = 3};
    state->geometryMask = NULL;
    state->geometryMaskW = 0;
    state->geometryMaskH = 0;

    fclose(f);
    return true;
}

bool input_is_video_path(const char *path)
{
    static const char *exts[] = {
        ".mp4", ".avi", ".mkv", ".webm", ".mov", ".flv", ".ts", ".m4v", NULL
    };
    size_t n = strlen(path);
    int i;
    for (i = 0; exts[i]; i++) {
        size_t elen = strlen(exts[i]);
        if (n >= elen && strcasecmp(path + n - elen, exts[i]) == 0) return true;
    }
    return false;
}

static int rt_producer_thread(void *userdata)
{
    AppState *state = (AppState *)userdata;
    uint8_t *tmp;
    uint8_t *dst_data[4];
    int      dst_linesize[4];
    int ret;

    tmp = (uint8_t *)malloc((size_t)g_rt.slot_bytes);
    if (!tmp) {
        g_rt.disconnected = 1;
        return 1;
    }

    dst_data[0] = tmp;
    dst_data[1] = NULL;
    dst_data[2] = NULL;
    dst_data[3] = NULL;
    dst_linesize[0] = g_rt.width * 3;
    dst_linesize[1] = 0;
    dst_linesize[2] = 0;
    dst_linesize[3] = 0;

    while (!g_rt.stop_flag) {
        ret = avcodec_receive_frame(g_rt.codec_ctx, g_rt.frame);
        if (ret == 0) {
            sws_scale(g_rt.sws_ctx,
                      (const uint8_t * const *)g_rt.frame->data, g_rt.frame->linesize,
                      0, g_rt.codec_ctx->height, dst_data, dst_linesize);
            av_frame_unref(g_rt.frame);

            SDL_LockMutex(g_rt.mutex);
            memcpy(g_rt.slots[g_rt.write_idx], tmp, (size_t)g_rt.slot_bytes);
            g_rt.write_idx = (g_rt.write_idx + 1) % RT_BUF_COUNT;
            if (g_rt.count < RT_BUF_COUNT) {
                g_rt.count++;
            } else {
                g_rt.dropped++;
            }
            SDL_CondSignal(g_rt.cond);
            SDL_UnlockMutex(g_rt.mutex);
            continue;
        }
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret != AVERROR(EAGAIN)) {
            break;
        }

        ret = av_read_frame(g_rt.fmt_ctx, g_rt.packet);
        if (ret < 0) {
            break;
        }
        if (g_rt.packet->stream_index != g_rt.video_stream_idx) {
            av_packet_unref(g_rt.packet);
            continue;
        }
        ret = avcodec_send_packet(g_rt.codec_ctx, g_rt.packet);
        av_packet_unref(g_rt.packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            break;
        }
    }

    free(tmp);
    if (!g_rt.stop_flag) {
        SDL_LockMutex(g_rt.mutex);
        g_rt.disconnected = 1;
        SDL_CondSignal(g_rt.cond);
        SDL_UnlockMutex(g_rt.mutex);
    }
    (void)state;
    return 0;
}

void input_close_source(AppState *state) {
    if (state->sourceType == SOURCE_REALTIME_STREAM) {
        int i;
        g_rt.stop_flag = 1;
        if (g_rt.mutex) {
            SDL_LockMutex(g_rt.mutex);
            SDL_CondSignal(g_rt.cond);
            SDL_UnlockMutex(g_rt.mutex);
        }
        if (g_rt.thread) {
            SDL_WaitThread(g_rt.thread, NULL);
            g_rt.thread = NULL;
        }
        av_frame_free(&g_rt.frame);
        av_packet_free(&g_rt.packet);
        if (g_rt.sws_ctx) {
            sws_freeContext(g_rt.sws_ctx);
            g_rt.sws_ctx = NULL;
        }
        avcodec_free_context(&g_rt.codec_ctx);
        avformat_close_input(&g_rt.fmt_ctx);
        for (i = 0; i < RT_BUF_COUNT; i++) {
            free(g_rt.slots[i]);
            g_rt.slots[i] = NULL;
        }
        if (g_rt.cond) {
            SDL_DestroyCond(g_rt.cond);
            g_rt.cond = NULL;
        }
        if (g_rt.mutex) {
            SDL_DestroyMutex(g_rt.mutex);
            g_rt.mutex = NULL;
        }
        memset(&g_rt, 0, sizeof g_rt);
        state->realtime = (RealtimeInfo){0};
    }
    if (state->sourceType == SOURCE_VIDEO_FILE) {
        if (g_vid.codec_ctx) avcodec_flush_buffers(g_vid.codec_ctx);
        av_frame_free(&g_vid.frame);
        av_packet_free(&g_vid.packet);
        if (g_vid.sws_ctx) { sws_freeContext(g_vid.sws_ctx); g_vid.sws_ctx = NULL; }
        avcodec_free_context(&g_vid.codec_ctx);
        avformat_close_input(&g_vid.fmt_ctx);
        memset(&g_vid, 0, sizeof g_vid);
        state->video = (VideoState){0};
    }
    free(state->inImage.data);
    free(state->outImage.data);
    free(state->geometryMask);
    free(state->preRotateImage.data);
    state->inImage        = (ImageBuffer){0};
    state->outImage       = (ImageBuffer){0};
    state->preRotateImage = (ImageBuffer){0};
    state->imgRotateAngle = 0.0f;
    state->imgFlipH       = false;
    state->imgFlipV       = false;
    state->geometryMask = NULL;
    state->geometryMaskW = 0;
    state->geometryMaskH = 0;
    state->sourceType = SOURCE_NONE;
}

bool input_open_video(AppState *state, const char *path, char *err, size_t err_len)
{
    AVStream *stream = NULL;
    const AVCodec *decoder = NULL;
    AVCodecParameters *codecpar = NULL;
    uint8_t *in_buf = NULL;
    uint8_t *out_buf = NULL;
    size_t byte_count;
    double fps;
    int i;
    int w;
    int h;
    /* Save previous state so we can restore on failure */
    ImageBuffer saved_in   = state->inImage;
    ImageBuffer saved_out  = state->outImage;
    SourceType  saved_type = state->sourceType;
    VideoState  saved_vid  = state->video;

    /* Zero pixel-buffer references before close so the close doesn't free them */
    state->inImage  = (ImageBuffer){0};
    state->outImage = (ImageBuffer){0};
    /* Close FFmpeg context (if was video) and reset sourceType to NONE */
    input_close_source(state);
    g_vid.video_stream_idx = -1;

    if (avformat_open_input(&g_vid.fmt_ctx, path, NULL, NULL) < 0) {
        snprintf(err, err_len, "input_open_video: cannot open '%s'", path);
        goto fail;
    }
    if (avformat_find_stream_info(g_vid.fmt_ctx, NULL) < 0) {
        snprintf(err, err_len, "input_open_video: cannot read stream info");
        goto fail;
    }

    for (i = 0; i < (int)g_vid.fmt_ctx->nb_streams; i++) {
        if (g_vid.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            g_vid.video_stream_idx = i;
            stream = g_vid.fmt_ctx->streams[i];
            break;
        }
    }
    if (stream == NULL) {
        snprintf(err, err_len, "input_open_video: no video stream");
        goto fail;
    }

    codecpar = stream->codecpar;
    decoder = avcodec_find_decoder(codecpar->codec_id);
    if (decoder == NULL) {
        snprintf(err, err_len, "input_open_video: unsupported video codec");
        goto fail;
    }

    g_vid.codec_ctx = avcodec_alloc_context3(decoder);
    if (g_vid.codec_ctx == NULL) {
        snprintf(err, err_len, "input_open_video: out of memory");
        goto fail;
    }
    if (avcodec_parameters_to_context(g_vid.codec_ctx, codecpar) < 0) {
        snprintf(err, err_len, "input_open_video: codec parameter copy failed");
        goto fail;
    }
    if (avcodec_open2(g_vid.codec_ctx, decoder, NULL) < 0) {
        snprintf(err, err_len, "input_open_video: decoder open failed");
        goto fail;
    }

    w = g_vid.codec_ctx->width;
    h = g_vid.codec_ctx->height;
    if (w <= 0 || h <= 0) {
        snprintf(err, err_len, "input_open_video: invalid dimensions %dx%d", w, h);
        goto fail;
    }

    g_vid.sws_ctx = sws_getContext(w, h, g_vid.codec_ctx->pix_fmt,
                                   w, h, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, NULL, NULL, NULL);
    if (g_vid.sws_ctx == NULL) {
        snprintf(err, err_len, "input_open_video: sws context allocation failed");
        goto fail;
    }

    g_vid.frame = av_frame_alloc();
    g_vid.packet = av_packet_alloc();
    if (g_vid.frame == NULL || g_vid.packet == NULL) {
        snprintf(err, err_len, "input_open_video: out of memory");
        goto fail;
    }

    byte_count = (size_t)w * (size_t)h * 3u;
    in_buf = (uint8_t *)malloc(byte_count);
    out_buf = (uint8_t *)malloc(byte_count);
    if (in_buf == NULL || out_buf == NULL) {
        snprintf(err, err_len, "input_open_video: out of memory");
        goto fail;
    }
    memset(in_buf, 0, byte_count);
    memset(out_buf, 0, byte_count);

    g_vid.time_base_s = av_q2d(stream->time_base);
    fps = av_q2d(stream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = av_q2d(stream->r_frame_rate);
    }
    g_vid.frame_dur_s = (fps > 0.0) ? (1.0 / fps) : (1.0 / 30.0);
    g_vid.wall_ref_s = 0.0;
    g_vid.pts_ref_s = 0.0;

    state->inImage = (ImageBuffer){.data = in_buf, .width = w, .height = h, .channels = 3};
    state->outImage = (ImageBuffer){.data = out_buf, .width = w, .height = h, .channels = 3};
    state->sourceType = SOURCE_VIDEO_FILE;
    state->effectCount = 0;
    state->video = (VideoState){.playing = false,
                                .loop = false,
                                .speed = 1.0f,
                                .current_pts_s = 0.0,
                                .duration_s = 0.0};
    if (g_vid.fmt_ctx->duration != AV_NOPTS_VALUE && g_vid.fmt_ctx->duration > 0) {
        state->video.duration_s = (double)g_vid.fmt_ctx->duration / (double)AV_TIME_BASE;
    }

    /* Success: discard saved buffers, they are replaced by the new ones */
    free(saved_in.data);
    free(saved_out.data);
    return true;

fail:
    free(in_buf);
    free(out_buf);
    av_frame_free(&g_vid.frame);
    av_packet_free(&g_vid.packet);
    if (g_vid.sws_ctx) {
        sws_freeContext(g_vid.sws_ctx);
        g_vid.sws_ctx = NULL;
    }
    avcodec_free_context(&g_vid.codec_ctx);
    avformat_close_input(&g_vid.fmt_ctx);
    memset(&g_vid, 0, sizeof g_vid);
    /* Restore previous state (pixel buffers survive; FFmpeg context is gone
     * if the prior source was a video file) */
    state->inImage    = saved_in;
    state->outImage   = saved_out;
    state->sourceType = saved_type;
    state->video      = saved_vid;
    return false;
}

static bool input_video_decode_one(AppState *state, bool use_timing, double target_pts_s)
{
    int ret;
    int64_t pts_ticks;
    double pts_s;
    uint8_t *dst_data[4];
    int dst_linesize[4];

    if (state == NULL || g_vid.fmt_ctx == NULL || g_vid.codec_ctx == NULL ||
        g_vid.frame == NULL || g_vid.packet == NULL || state->inImage.data == NULL) {
        return false;
    }

    while (true) {
        ret = avcodec_receive_frame(g_vid.codec_ctx, g_vid.frame);
        if (ret == 0) {
            pts_ticks = g_vid.frame->best_effort_timestamp;
            if (pts_ticks == AV_NOPTS_VALUE) {
                pts_ticks = g_vid.frame->pts;
            }
            if (pts_ticks == AV_NOPTS_VALUE) {
                pts_ticks = 0;
            }
            pts_s = (double)pts_ticks * g_vid.time_base_s;
            if (use_timing && pts_s + (g_vid.frame_dur_s * 0.5) < target_pts_s) {
                av_frame_unref(g_vid.frame);
                continue;
            }

            dst_data[0] = state->inImage.data;
            dst_data[1] = NULL;
            dst_data[2] = NULL;
            dst_data[3] = NULL;
            dst_linesize[0] = state->inImage.width * 3;
            dst_linesize[1] = 0;
            dst_linesize[2] = 0;
            dst_linesize[3] = 0;
            sws_scale(g_vid.sws_ctx,
                      (const uint8_t * const *)g_vid.frame->data,
                      g_vid.frame->linesize,
                      0,
                      g_vid.codec_ctx->height,
                      dst_data,
                      dst_linesize);
            state->video.current_pts_s = pts_s;
            av_frame_unref(g_vid.frame);
            return true;
        }

        if (ret == AVERROR_EOF) {
            if (state->video.loop) {
                input_video_seek(state, 0.0f);
            } else {
                state->video.playing = false;
                state->video.at_end = true;
            }
            return false;
        }
        if (ret != AVERROR(EAGAIN)) {
            return false;
        }

        ret = av_read_frame(g_vid.fmt_ctx, g_vid.packet);
        if (ret < 0) {
            if (state->video.loop) {
                input_video_seek(state, 0.0f);
            } else {
                state->video.playing = false;
                state->video.at_end = true;
            }
            return false;
        }
        if (g_vid.packet->stream_index != g_vid.video_stream_idx) {
            av_packet_unref(g_vid.packet);
            continue;
        }

        ret = avcodec_send_packet(g_vid.codec_ctx, g_vid.packet);
        av_packet_unref(g_vid.packet);
        if (ret < 0 && ret != AVERROR(EAGAIN)) {
            return false;
        }
    }
}

bool input_read_next_frame(AppState *state)
{
    double now_s;
    double target_pts_s;

    if (state == NULL || state->sourceType != SOURCE_VIDEO_FILE ||
        !state->video.playing || g_vid.fmt_ctx == NULL) {
        return false;
    }

    now_s = get_wall_s();
    if (g_vid.wall_ref_s == 0.0) {
        g_vid.wall_ref_s = now_s;
        g_vid.pts_ref_s = state->video.current_pts_s;
    }

    target_pts_s = g_vid.pts_ref_s +
                   (now_s - g_vid.wall_ref_s) * (double)state->video.speed;
    if (target_pts_s < state->video.current_pts_s + (g_vid.frame_dur_s * 0.5)) {
        return false;
    }

    return input_video_decode_one(state, true, target_pts_s);
}

void input_video_seek(AppState *state, float frac)
{
    double duration_s;
    double target_s;
    int64_t ts;

    if (state == NULL || state->sourceType != SOURCE_VIDEO_FILE ||
        g_vid.fmt_ctx == NULL || g_vid.codec_ctx == NULL ||
        g_vid.time_base_s <= 0.0) {
        return;
    }
    if (frac > 0.0f && state->video.duration_s <= 0.0) {
        return;
    }

    if (frac < 0.0f) {
        frac = 0.0f;
    } else if (frac > 1.0f) {
        frac = 1.0f;
    }

    duration_s = state->video.duration_s;
    target_s = duration_s * (double)frac;
    ts = (int64_t)(target_s / g_vid.time_base_s);
    if (av_seek_frame(g_vid.fmt_ctx, g_vid.video_stream_idx, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
        avcodec_flush_buffers(g_vid.codec_ctx);
        state->video.at_end = false;
        state->video.current_pts_s = target_s;
        input_video_reset_timing();
    }
}

void input_video_seek_end(AppState *state)
{
    double frac;
    double target_s;

    if (state == NULL || state->video.duration_s <= 0.0) {
        return;
    }
    target_s = state->video.duration_s - 0.5;
    if (target_s < 0.0) {
        target_s = 0.0;
    }
    frac = target_s / state->video.duration_s;
    input_video_seek(state, (float)frac);
    state->video.playing = false;
}

void input_video_reset_timing(void)
{
    g_vid.wall_ref_s = 0.0;
    g_vid.pts_ref_s = 0.0;
}

void input_video_step_frame(AppState *state)
{
    (void)input_video_decode_one(state, false, 0.0);
}

void input_video_step_frame_backward(AppState *state)
{
    double target_s;
    double seek_s;
    float frac;

    if (state == NULL || state->video.duration_s <= 0.0 || g_vid.frame_dur_s <= 0.0) {
        return;
    }
    target_s = state->video.current_pts_s - g_vid.frame_dur_s;
    if (target_s < 0.0) {
        target_s = 0.0;
    }
    seek_s = target_s - g_vid.frame_dur_s * 2.0;
    if (seek_s < 0.0) {
        seek_s = 0.0;
    }
    frac = (float)(seek_s / state->video.duration_s);
    input_video_seek(state, frac);
    (void)input_video_decode_one(state, true, target_s);
}

bool input_open_realtime_stream(AppState *state, const char *url, char *err, size_t err_len)
{
    static bool avdevice_inited = false;
    const AVInputFormat *ifmt = NULL;
    AVDictionary *opts = NULL;
    AVStream *stream = NULL;
    const AVCodec *decoder = NULL;
    AVCodecParameters *codecpar = NULL;
    uint8_t *in_buf = NULL;
    uint8_t *out_buf = NULL;
    int i, w, h;
    double fps;
    size_t byte_count;
    ImageBuffer saved_in   = state->inImage;
    ImageBuffer saved_out  = state->outImage;
    SourceType  saved_type = state->sourceType;
    RealtimeInfo saved_rt  = state->realtime;

    if (!avdevice_inited) {
        avdevice_register_all();
        avdevice_inited = true;
    }

    state->inImage  = (ImageBuffer){0};
    state->outImage = (ImageBuffer){0};
    input_close_source(state);

    if (strncmp(url, "/dev/video", 10) == 0) {
        ifmt = av_find_input_format("v4l2");
        if (!ifmt) {
            snprintf(err, err_len, "v4l2 not available");
            goto fail;
        }
        av_dict_set(&opts, "framerate", "30", 0);
        av_dict_set(&opts, "video_size", "640x480", 0);
    } else {
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);
        av_dict_set(&opts, "timeout", "5000000", 0);
    }

    g_rt.video_stream_idx = -1;
    if (avformat_open_input(&g_rt.fmt_ctx, url, ifmt, &opts) < 0) {
        av_dict_free(&opts);
        snprintf(err, err_len, "input_open_realtime_stream: cannot open '%s'", url);
        goto fail;
    }
    av_dict_free(&opts);

    if (avformat_find_stream_info(g_rt.fmt_ctx, NULL) < 0) {
        snprintf(err, err_len, "input_open_realtime_stream: cannot read stream info");
        goto fail;
    }

    for (i = 0; i < (int)g_rt.fmt_ctx->nb_streams; i++) {
        if (g_rt.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            g_rt.video_stream_idx = i;
            stream = g_rt.fmt_ctx->streams[i];
            break;
        }
    }
    if (!stream) {
        snprintf(err, err_len, "input_open_realtime_stream: no video stream");
        goto fail;
    }

    codecpar = stream->codecpar;
    decoder  = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        snprintf(err, err_len, "input_open_realtime_stream: unsupported codec");
        goto fail;
    }

    g_rt.codec_ctx = avcodec_alloc_context3(decoder);
    if (!g_rt.codec_ctx) {
        snprintf(err, err_len, "input_open_realtime_stream: OOM");
        goto fail;
    }
    if (avcodec_parameters_to_context(g_rt.codec_ctx, codecpar) < 0) {
        snprintf(err, err_len, "input_open_realtime_stream: codec param copy failed");
        goto fail;
    }
    if (avcodec_open2(g_rt.codec_ctx, decoder, NULL) < 0) {
        snprintf(err, err_len, "input_open_realtime_stream: decoder open failed");
        goto fail;
    }

    w = g_rt.codec_ctx->width;
    h = g_rt.codec_ctx->height;
    if (w <= 0 || h <= 0) {
        snprintf(err, err_len, "input_open_realtime_stream: invalid dimensions %dx%d", w, h);
        goto fail;
    }

    g_rt.sws_ctx = sws_getContext(w, h, g_rt.codec_ctx->pix_fmt,
                                   w, h, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, NULL, NULL, NULL);
    if (!g_rt.sws_ctx) {
        snprintf(err, err_len, "input_open_realtime_stream: sws alloc failed");
        goto fail;
    }

    g_rt.frame  = av_frame_alloc();
    g_rt.packet = av_packet_alloc();
    if (!g_rt.frame || !g_rt.packet) {
        snprintf(err, err_len, "input_open_realtime_stream: OOM");
        goto fail;
    }

    fps = av_q2d(stream->avg_frame_rate);
    if (fps <= 0.0) {
        fps = av_q2d(stream->r_frame_rate);
    }
    if (fps <= 0.0) {
        fps = 30.0;
    }

    g_rt.width      = w;
    g_rt.height     = h;
    g_rt.fps        = (float)fps;
    g_rt.slot_bytes = w * h * 3;

    for (i = 0; i < RT_BUF_COUNT; i++) {
        g_rt.slots[i] = (uint8_t *)malloc((size_t)g_rt.slot_bytes);
        if (!g_rt.slots[i]) {
            snprintf(err, err_len, "input_open_realtime_stream: OOM");
            goto fail;
        }
        memset(g_rt.slots[i], 0, (size_t)g_rt.slot_bytes);
    }

    g_rt.mutex = SDL_CreateMutex();
    g_rt.cond  = SDL_CreateCond();
    if (!g_rt.mutex || !g_rt.cond) {
        snprintf(err, err_len, "input_open_realtime_stream: SDL sync alloc failed");
        goto fail;
    }

    byte_count = (size_t)w * (size_t)h * 3u;
    in_buf  = (uint8_t *)malloc(byte_count);
    out_buf = (uint8_t *)malloc(byte_count);
    if (!in_buf || !out_buf) {
        snprintf(err, err_len, "input_open_realtime_stream: OOM");
        goto fail;
    }
    memset(in_buf,  0, byte_count);
    memset(out_buf, 0, byte_count);

    snprintf(g_rt.url, sizeof g_rt.url, "%s", url);
    g_rt.stop_flag    = 0;
    g_rt.disconnected = 0;
    g_rt.write_idx    = 0;
    g_rt.count        = 0;
    g_rt.dropped      = 0;

    g_rt.thread = SDL_CreateThread(rt_producer_thread, "rt_producer", state);
    if (!g_rt.thread) {
        snprintf(err, err_len, "input_open_realtime_stream: SDL_CreateThread failed");
        goto fail;
    }

    state->inImage  = (ImageBuffer){.data = in_buf,  .width = w, .height = h, .channels = 3};
    state->outImage = (ImageBuffer){.data = out_buf, .width = w, .height = h, .channels = 3};
    state->sourceType   = SOURCE_REALTIME_STREAM;
    state->effectCount  = 0;
    state->realtime = (RealtimeInfo){
        .playing      = true,
        .disconnected = false,
        .width        = w,
        .height       = h,
        .fps          = (float)fps
    };

    free(saved_in.data);
    free(saved_out.data);
    return true;

fail:
    free(in_buf);
    free(out_buf);
    {
        int j;
        for (j = 0; j < RT_BUF_COUNT; j++) {
            free(g_rt.slots[j]);
            g_rt.slots[j] = NULL;
        }
    }
    if (g_rt.cond) {
        SDL_DestroyCond(g_rt.cond);
        g_rt.cond  = NULL;
    }
    if (g_rt.mutex) {
        SDL_DestroyMutex(g_rt.mutex);
        g_rt.mutex = NULL;
    }
    av_frame_free(&g_rt.frame);
    av_packet_free(&g_rt.packet);
    if (g_rt.sws_ctx) {
        sws_freeContext(g_rt.sws_ctx);
        g_rt.sws_ctx = NULL;
    }
    avcodec_free_context(&g_rt.codec_ctx);
    avformat_close_input(&g_rt.fmt_ctx);
    memset(&g_rt, 0, sizeof g_rt);

    state->inImage    = saved_in;
    state->outImage   = saved_out;
    state->sourceType = saved_type;
    state->realtime   = saved_rt;
    return false;
}

bool input_read_realtime_frame(AppState *state)
{
    int newest;

    if (!state || state->sourceType != SOURCE_REALTIME_STREAM || !state->realtime.playing) {
        return false;
    }

    if (g_rt.disconnected && !state->realtime.disconnected) {
        state->realtime.disconnected = true;
        fprintf(stderr, "realtime: stream disconnected (press p to reconnect)\n");
    }

    if (!g_rt.mutex) {
        return false;
    }

    SDL_LockMutex(g_rt.mutex);
    if (g_rt.count == 0) {
        SDL_UnlockMutex(g_rt.mutex);
        return false;
    }
    newest = (g_rt.write_idx - 1 + RT_BUF_COUNT) % RT_BUF_COUNT;
    memcpy(state->inImage.data, g_rt.slots[newest], (size_t)g_rt.slot_bytes);
    g_rt.count = 0;
    SDL_UnlockMutex(g_rt.mutex);
    return true;
}

void input_reconnect_realtime_stream(AppState *state)
{
    char url[512];
    char err[256];

    if (!state || state->sourceType != SOURCE_REALTIME_STREAM) {
        return;
    }
    snprintf(url, sizeof url, "%s", g_rt.url);
    printf("reconnecting to: %s\n", url);
    if (!input_open_realtime_stream(state, url, err, sizeof err)) {
        fprintf(stderr, "reconnect failed: %s\n", err);
    } else {
        printf("reconnected: %dx%d @ %.1ffps\n",
               state->realtime.width, state->realtime.height, (double)state->realtime.fps);
    }
}

void input_realtime_status(int *count, int *cap, uint64_t *dropped)
{
    if (!g_rt.mutex) {
        *count = 0;
        *cap = RT_BUF_COUNT;
        *dropped = 0;
        return;
    }
    SDL_LockMutex(g_rt.mutex);
    *count   = g_rt.count;
    *dropped = g_rt.dropped;
    SDL_UnlockMutex(g_rt.mutex);
    *cap = RT_BUF_COUNT;
}
