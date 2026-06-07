#pragma once

#include "ImageBuffer.h"
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <opencv2/videoio.hpp>

class QTimer;

// Drives video file playback on the GUI thread via QTimer.
// File and stream sources use OpenCV VideoCapture. Stream sources keep the
// latest-frame handoff and reconnect/status policy from the previous producer.
class VideoInputService : public QObject {
    Q_OBJECT
public:
    enum class StreamStatus { Disconnected, Connecting, Connected, Reconnecting };
    Q_ENUM(StreamStatus)

    explicit VideoInputService(QObject *parent = nullptr);
    ~VideoInputService() override;

    // Open/close
    bool open(const QString &path);  // true on success; emits errorOccurred on failure
    void close();
    bool isOpen() const;

    bool openStream(const QString &url);
    void closeStream();
    bool isStream() const;
    StreamStatus streamStatus() const;
    void reconnectStream();

    // File metadata (valid after open())
    double durationSecs() const;
    double fps() const;
    int videoWidth() const;
    int videoHeight() const;

    // Playback control
    void play();
    void pause();
    bool isPlaying() const;

    void setLoop(bool loop);
    bool loop() const;

    void setSpeed(double speed);   // 0.5 / 1.0 / 2.0 etc.
    double speed() const;

    void seekToSecs(double secs);
    void stepForward();            // decode and deliver 1 frame; does not change play state
    void stepBackward();           // seek back ~1 frame

    double currentTimeSecs() const;

signals:
    void frameReady(std::shared_ptr<ImageBuffer> frame);  // always on GUI thread
    void positionChanged(double secs);
    void playbackFinished();                              // EOF reached and loop is off
    void errorOccurred(const QString &message);
    void streamStatusChanged(VideoInputService::StreamStatus status);

private slots:
    void onTimerTick();

private:
    // Decode one frame and return it, or nullptr on EOF/error.
    std::shared_ptr<ImageBuffer> decodeNextFrame();
    std::shared_ptr<ImageBuffer> decodeVideoCaptureFrame();
    std::shared_ptr<ImageBuffer> toImageBuffer(const cv::Mat &frame);

    // Seek to timestamp in seconds (flushes decoder buffers).
    void seekInternal(double secs);
    void runStreamProducer();
    void onDisplayTick();
    void onStreamDisconnected();
    void setStreamStatus(StreamStatus s);

    cv::VideoCapture fileCapture_;
    cv::VideoCapture streamCapture_;

    double durationSecs_    = 0.0;
    double fps_             = 25.0;
    double currentTimeSecs_ = 0.0;
    bool   loop_            = true;
    double speed_           = 1.0;

    QTimer *playTimer_ = nullptr;
    QString streamUrl_;
    bool isStream_ = false;
    StreamStatus streamStatus_ = StreamStatus::Disconnected;
    std::thread producerThread_;
    std::atomic<bool> stopProducer_{false};
    std::mutex latestMutex_;
    std::shared_ptr<ImageBuffer> latestFrame_;
    QTimer *displayTimer_ = nullptr;
    QTimer *reconnectTimer_ = nullptr;
};
