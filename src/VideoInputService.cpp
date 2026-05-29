#include "VideoInputService.h"
#include <QTimer>
#include <algorithm>

VideoInputService::VideoInputService(QObject *parent)
    : QObject(parent)
{
    av_register_all();
    playTimer_ = new QTimer(this);
    connect(playTimer_, &QTimer::timeout, this, &VideoInputService::onTimerTick);
}

VideoInputService::~VideoInputService()
{
    close();
}

bool VideoInputService::open(const QString &path)
{
    close();

    // Open input file
    int ret = avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        emit errorOccurred(QString("Failed to open video file: %1").arg(path));
        return false;
    }

    // Find stream info
    ret = avformat_find_stream_info(fmtCtx_, nullptr);
    if (ret < 0) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("Failed to find stream info");
        return false;
    }

    // Find video stream
    videoStreamIdx_ = -1;
    for (unsigned int i = 0; i < fmtCtx_->nb_streams; ++i) {
        if (fmtCtx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx_ = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIdx_ < 0) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("No video stream found");
        return false;
    }

    // Get codec
    AVCodec *codec = avcodec_find_decoder(fmtCtx_->streams[videoStreamIdx_]->codec->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("Codec not found");
        return false;
    }

    // Allocate codec context and copy codec parameters
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("Failed to allocate codec context");
        return false;
    }

    avcodec_copy_context(codecCtx_, fmtCtx_->streams[videoStreamIdx_]->codec);

    // Open codec
    ret = avcodec_open2(codecCtx_, codec, nullptr);
    if (ret < 0) {
        av_free(codecCtx_);
        codecCtx_ = nullptr;
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("Failed to open codec");
        return false;
    }

    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
        avcodec_close(codecCtx_);
        av_free(codecCtx_);
        codecCtx_ = nullptr;
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
        emit errorOccurred("Failed to allocate frame");
        return false;
    }

    // Compute FPS from avg_frame_rate (AVRational: num/den)
    AVRational frameRate = fmtCtx_->streams[videoStreamIdx_]->avg_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0) {
        fps_ = static_cast<double>(frameRate.num) / static_cast<double>(frameRate.den);
    } else {
        fps_ = 25.0;  // fallback
    }

    // Compute duration in seconds
    if (fmtCtx_->duration > 0) {
        durationSecs_ = static_cast<double>(fmtCtx_->duration) / static_cast<double>(AV_TIME_BASE);
    } else {
        durationSecs_ = 0.0;
    }

    currentTimeSecs_ = 0.0;
    return true;
}

void VideoInputService::close()
{
    playTimer_->stop();

    if (codecCtx_) {
        avcodec_close(codecCtx_);
        av_free(codecCtx_);
        codecCtx_ = nullptr;
    }

    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }

    if (frame_) {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    videoStreamIdx_ = -1;
    currentTimeSecs_ = 0.0;
    fps_ = 25.0;
    durationSecs_ = 0.0;
}

bool VideoInputService::isOpen() const
{
    return fmtCtx_ != nullptr && codecCtx_ != nullptr && frame_ != nullptr;
}

double VideoInputService::durationSecs() const
{
    return durationSecs_;
}

double VideoInputService::fps() const
{
    return fps_;
}

int VideoInputService::videoWidth() const
{
    return codecCtx_ ? codecCtx_->width : 0;
}

int VideoInputService::videoHeight() const
{
    return codecCtx_ ? codecCtx_->height : 0;
}

void VideoInputService::play()
{
    if (!isOpen()) return;
    int interval = std::max(1, static_cast<int>(1000.0 / fps_ / speed_));
    playTimer_->start(interval);
}

void VideoInputService::pause()
{
    playTimer_->stop();
}

bool VideoInputService::isPlaying() const
{
    return playTimer_->isActive();
}

void VideoInputService::setLoop(bool loop)
{
    loop_ = loop;
}

bool VideoInputService::loop() const
{
    return loop_;
}

void VideoInputService::setSpeed(double speed)
{
    speed_ = speed;
    if (isPlaying()) {
        playTimer_->stop();
        play();  // restart with new speed
    }
}

double VideoInputService::speed() const
{
    return speed_;
}

void VideoInputService::seekToSecs(double secs)
{
    seekInternal(secs);
    stepForward();
}

void VideoInputService::stepForward()
{
    auto frame = decodeNextFrame();
    if (frame) {
        emit frameReady(frame);
        emit positionChanged(currentTimeSecs_);
    }
}

void VideoInputService::stepBackward()
{
    double target = std::max(0.0, currentTimeSecs_ - 2.0 / fps_);
    seekInternal(target);
    stepForward();
}

double VideoInputService::currentTimeSecs() const
{
    return currentTimeSecs_;
}

std::shared_ptr<ImageBuffer> VideoInputService::decodeNextFrame()
{
    if (!isOpen()) return nullptr;

    while (true) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        int ret = av_read_frame(fmtCtx_, &pkt);
        if (ret < 0) {
            // EOF or error
            return nullptr;
        }

        // Skip packets from other streams
        if (pkt.stream_index != videoStreamIdx_) {
            av_free_packet(&pkt);
            continue;
        }

        int gotFrame = 0;
        avcodec_decode_video2(codecCtx_, frame_, &gotFrame, &pkt);
        av_free_packet(&pkt);

        if (gotFrame) {
            // Lazy-create or recreate sws context using actual frame dimensions/format
            AVPixelFormat srcFmt = static_cast<AVPixelFormat>(frame_->format);
            int w = frame_->width  > 0 ? frame_->width  : codecCtx_->width;
            int h = frame_->height > 0 ? frame_->height : codecCtx_->height;
            if (!swsCtx_) {
                swsCtx_ = sws_getContext(w, h, srcFmt,
                                         w, h, AV_PIX_FMT_RGB24,
                                         SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!swsCtx_) {
                    continue;  // skip this frame, try next
                }
            }

            auto buf = std::make_shared<ImageBuffer>();
            buf->width = w;
            buf->height = h;
            buf->channels = 3;
            buf->data.resize(static_cast<std::size_t>(buf->width * buf->height * 3));

            uint8_t *dst[1] = {buf->data.data()};
            int dstStride[1] = {buf->width * 3};

            sws_scale(swsCtx_, frame_->data, frame_->linesize, 0, h,
                      dst, dstStride);

            // Update timestamp
            AVStream *stream = fmtCtx_->streams[videoStreamIdx_];
            int64_t best_effort_ts = frame_->best_effort_timestamp;
            if (best_effort_ts != AV_NOPTS_VALUE) {
                currentTimeSecs_ = static_cast<double>(best_effort_ts) * av_q2d(stream->time_base);
            }

            return buf;
        }
    }
}

void VideoInputService::seekInternal(double secs)
{
    if (!isOpen()) return;

    AVStream *stream = fmtCtx_->streams[videoStreamIdx_];
    int64_t ts = static_cast<int64_t>(secs / av_q2d(stream->time_base));
    av_seek_frame(fmtCtx_, videoStreamIdx_, ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx_);
    currentTimeSecs_ = secs;
}

void VideoInputService::onTimerTick()
{
    auto frame = decodeNextFrame();

    if (!frame) {
        // EOF reached
        if (loop_) {
            seekInternal(0.0);
            return;  // Next tick will deliver first frame
        } else {
            pause();
            emit playbackFinished();
            return;
        }
    }

    emit frameReady(frame);
    emit positionChanged(currentTimeSecs_);
}
