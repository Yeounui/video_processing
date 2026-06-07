#include "VideoInputService.h"
#include "OpenCvImageBridge.h"
#include <QMetaObject>
#include <QTimer>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {
constexpr int kStreamOpenTimeoutMsec = 3000;
constexpr int kStreamReadTimeoutMsec = 1000;

std::string toOpenCvPath(const QString &path)
{
    return path.toUtf8().toStdString();
}

double validFrameRateOrFallback(double fps)
{
    return std::isfinite(fps) && fps > 0.0 ? fps : 25.0;
}

bool looksLikeNetworkUrl(const QString &url)
{
    return url.contains(QStringLiteral("://"));
}

std::chrono::milliseconds frameIntervalForFps(double fps)
{
    const double validFps = validFrameRateOrFallback(fps);
    return std::chrono::milliseconds(
        std::max(1, static_cast<int>(std::lround(1000.0 / validFps))));
}

bool openStreamCapture(cv::VideoCapture &capture, const QString &url)
{
    const std::vector<int> params = {
        cv::CAP_PROP_OPEN_TIMEOUT_MSEC, kStreamOpenTimeoutMsec,
        cv::CAP_PROP_READ_TIMEOUT_MSEC, kStreamReadTimeoutMsec,
    };
    const std::string source = toOpenCvPath(url);
    if (capture.open(source, cv::CAP_ANY, params)) {
        return true;
    }
    capture.release();

    if (!looksLikeNetworkUrl(url)) {
        return capture.open(source, cv::CAP_ANY);
    }

    return false;
}
}

VideoInputService::VideoInputService(QObject *parent)
    : QObject(parent)
{
    playTimer_ = new QTimer(this);
    connect(playTimer_, &QTimer::timeout, this, &VideoInputService::onTimerTick);

    displayTimer_ = new QTimer(this);
    displayTimer_->setInterval(33);
    connect(displayTimer_, &QTimer::timeout, this, &VideoInputService::onDisplayTick);

    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setSingleShot(true);
    reconnectTimer_->setInterval(3000);
    connect(reconnectTimer_, &QTimer::timeout, this, &VideoInputService::reconnectStream);
}

VideoInputService::~VideoInputService()
{
    closeStream();
    close();
}

bool VideoInputService::open(const QString &path)
{
    closeStream();
    close();

    if (!fileCapture_.open(toOpenCvPath(path), cv::CAP_ANY)) {
        emit errorOccurred(QString("Failed to open video file: %1").arg(path));
        return false;
    }

    fps_ = validFrameRateOrFallback(fileCapture_.get(cv::CAP_PROP_FPS));
    const double frameCount = fileCapture_.get(cv::CAP_PROP_FRAME_COUNT);
    if (std::isfinite(frameCount) && frameCount > 0.0 && fps_ > 0.0) {
        durationSecs_ = frameCount / fps_;
    } else {
        durationSecs_ = 0.0;
    }

    currentTimeSecs_ = 0.0;
    return true;
}

void VideoInputService::close()
{
    playTimer_->stop();
    fileCapture_.release();
    currentTimeSecs_ = 0.0;
    fps_ = 25.0;
    durationSecs_ = 0.0;
}

bool VideoInputService::openStream(const QString &url)
{
    closeStream();
    close();

    streamUrl_ = url;
    isStream_ = true;
    stopProducer_ = false;
    setStreamStatus(StreamStatus::Connecting);

    if (!openStreamCapture(streamCapture_, url)) {
        isStream_ = false;
        setStreamStatus(StreamStatus::Disconnected);
        emit errorOccurred(QString("Failed to open stream: %1").arg(url));
        return false;
    }

    fps_ = validFrameRateOrFallback(streamCapture_.get(cv::CAP_PROP_FPS));
    durationSecs_ = 0.0;
    {
        std::lock_guard<std::mutex> lock(latestMutex_);
        latestFrame_.reset();
    }

    currentTimeSecs_ = 0.0;
    setStreamStatus(StreamStatus::Connected);
    displayTimer_->start();
    producerThread_ = std::thread(&VideoInputService::runStreamProducer, this);
    return true;
}

void VideoInputService::closeStream()
{
    if (!isStream_) {
        return;
    }

    reconnectTimer_->stop();
    displayTimer_->stop();
    stopProducer_ = true;

    if (producerThread_.joinable()) {
        producerThread_.join();
    }

    stopProducer_ = false;
    isStream_ = false;
    streamCapture_.release();
    {
        std::lock_guard<std::mutex> lock(latestMutex_);
        latestFrame_.reset();
    }

    close();
    setStreamStatus(StreamStatus::Disconnected);
}

bool VideoInputService::isStream() const
{
    return isStream_;
}

VideoInputService::StreamStatus VideoInputService::streamStatus() const
{
    return streamStatus_;
}

void VideoInputService::reconnectStream()
{
    if (streamUrl_.isEmpty()) {
        return;
    }

    setStreamStatus(StreamStatus::Reconnecting);
    openStream(streamUrl_);
}

bool VideoInputService::isOpen() const
{
    if (!isStream_) {
        return fileCapture_.isOpened();
    }

    return streamCapture_.isOpened();
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
    if (fileCapture_.isOpened()) {
        return static_cast<int>(std::lround(fileCapture_.get(cv::CAP_PROP_FRAME_WIDTH)));
    }

    if (streamCapture_.isOpened()) {
        return static_cast<int>(std::lround(streamCapture_.get(cv::CAP_PROP_FRAME_WIDTH)));
    }

    return 0;
}

int VideoInputService::videoHeight() const
{
    if (fileCapture_.isOpened()) {
        return static_cast<int>(std::lround(fileCapture_.get(cv::CAP_PROP_FRAME_HEIGHT)));
    }

    if (streamCapture_.isOpened()) {
        return static_cast<int>(std::lround(streamCapture_.get(cv::CAP_PROP_FRAME_HEIGHT)));
    }

    return 0;
}

void VideoInputService::play()
{
    if (!isOpen() || isStream_) return;
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
    speed_ = std::isfinite(speed) && speed > 0.0 ? speed : 1.0;
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
    if (isStream_) {
        return;
    }

    seekInternal(secs);
    stepForward();
}

void VideoInputService::stepForward()
{
    if (isStream_) {
        return;
    }

    auto frame = decodeNextFrame();
    if (frame) {
        emit frameReady(frame);
        emit positionChanged(currentTimeSecs_);
    }
}

void VideoInputService::stepBackward()
{
    if (isStream_) {
        return;
    }

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
    if (!isStream_) {
        return decodeVideoCaptureFrame();
    }

    return nullptr;
}

std::shared_ptr<ImageBuffer> VideoInputService::decodeVideoCaptureFrame()
{
    cv::Mat frame;
    if (!fileCapture_.read(frame) || frame.empty()) {
        return nullptr;
    }

    auto buf = toImageBuffer(frame);
    if (!buf) {
        return nullptr;
    }

    const double posMillis = fileCapture_.get(cv::CAP_PROP_POS_MSEC);
    if (std::isfinite(posMillis) && posMillis > 0.0) {
        currentTimeSecs_ = posMillis / 1000.0;
    } else {
        const double posFrames = fileCapture_.get(cv::CAP_PROP_POS_FRAMES);
        if (std::isfinite(posFrames) && posFrames > 0.0 && fps_ > 0.0) {
            currentTimeSecs_ = (posFrames - 1.0) / fps_;
        }
    }

    return buf;
}

std::shared_ptr<ImageBuffer> VideoInputService::toImageBuffer(const cv::Mat &frame)
{
    if (frame.empty() || frame.depth() != CV_8U) {
        return nullptr;
    }

    cv::Mat boundary;
    OpenCvImageBridge::ColorOrder colorOrder = OpenCvImageBridge::ColorOrder::Bgr;
    switch (frame.channels()) {
    case 1:
        cv::cvtColor(frame, boundary, cv::COLOR_GRAY2RGB);
        colorOrder = OpenCvImageBridge::ColorOrder::Rgb;
        break;
    case 3:
    case 4:
        boundary = frame;
        colorOrder = OpenCvImageBridge::ColorOrder::Bgr;
        break;
    default:
        return nullptr;
    }

    auto buf = std::make_shared<ImageBuffer>();
    if (!OpenCvImageBridge::copyMatToImageBuffer(boundary, *buf, colorOrder)) {
        return nullptr;
    }
    return buf;
}

void VideoInputService::seekInternal(double secs)
{
    if (!isOpen()) return;
    if (!isStream_) {
        const double clampedSecs = std::max(0.0, secs);
        if (clampedSecs == 0.0) {
            fileCapture_.set(cv::CAP_PROP_POS_FRAMES, 0.0);
        } else {
            fileCapture_.set(cv::CAP_PROP_POS_MSEC, clampedSecs * 1000.0);
        }
        currentTimeSecs_ = clampedSecs;
        return;
    }
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

void VideoInputService::runStreamProducer()
{
    const auto frameInterval = frameIntervalForFps(fps_);

    while (!stopProducer_) {
        cv::Mat frame;
        if (!streamCapture_.read(frame) || frame.empty()) {
            if (!stopProducer_) {
                QMetaObject::invokeMethod(this, &VideoInputService::onStreamDisconnected,
                                          Qt::QueuedConnection);
            }
            break;
        }

        auto buf = toImageBuffer(frame);
        if (buf) {
            std::lock_guard<std::mutex> lock(latestMutex_);
            latestFrame_ = std::move(buf);
        }

        if (frameInterval.count() > 1) {
            std::this_thread::sleep_for(frameInterval);
        }
    }
}

void VideoInputService::onDisplayTick()
{
    std::shared_ptr<ImageBuffer> frame;
    {
        std::lock_guard<std::mutex> lock(latestMutex_);
        frame.swap(latestFrame_);
    }

    if (frame) {
        emit frameReady(frame);
    }
}

void VideoInputService::onStreamDisconnected()
{
    if (!isStream_) {
        return;
    }

    displayTimer_->stop();
    stopProducer_ = true;

    if (producerThread_.joinable()) {
        producerThread_.join();
    }

    stopProducer_ = false;

    {
        std::lock_guard<std::mutex> lock(latestMutex_);
        latestFrame_.reset();
    }

    close();
    streamCapture_.release();
    setStreamStatus(StreamStatus::Disconnected);

    if (!streamUrl_.isEmpty()) {
        setStreamStatus(StreamStatus::Reconnecting);
        reconnectTimer_->start();
    }
}

void VideoInputService::setStreamStatus(StreamStatus s)
{
    if (streamStatus_ == s) {
        return;
    }

    streamStatus_ = s;
    emit streamStatusChanged(s);
}
