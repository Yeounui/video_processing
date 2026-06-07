#include "VideoInputService.h"

#include <QtTest>

#include <QTemporaryDir>

#include <cmath>
#include <memory>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class TestVideoInputService : public QObject {
    Q_OBJECT

private slots:
    void testOpenCvFilePlaybackProducesRgbFrames();
    void testSeekToStartDeliversFirstFrame();
    void testTimerPlaybackStopsAtEofWhenLoopDisabled();
    void testTimerPlaybackLoopsWhenEnabled();
    void testInvalidSpeedFallsBackToNormalRate();
    void testMissingFileReportsError();
    void testOpenCvStreamProducesLatestFramesAndStatus();
    void testMissingStreamReportsError();
};

namespace {
bool writeTestVideo(const QString &path)
{
    cv::VideoWriter writer(path.toUtf8().toStdString(),
                           cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                           5.0,
                           cv::Size(4, 2),
                           true);
    if (!writer.isOpened()) {
        return false;
    }

    const std::vector<cv::Scalar> bgrFrames = {
        cv::Scalar(10, 20, 30),
        cv::Scalar(40, 50, 60),
        cv::Scalar(70, 80, 90),
    };
    for (const cv::Scalar &color : bgrFrames) {
        const cv::Mat frame(2, 4, CV_8UC3, color);
        writer.write(frame);
    }
    writer.release();

    cv::VideoCapture verifier(path.toUtf8().toStdString());
    return verifier.isOpened();
}

QString makeTestVideoOrEmpty(QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("source.avi"));
    return writeTestVideo(path) ? path : QString();
}

bool channelNear(int actual, int expected)
{
    return std::abs(actual - expected) <= 30;
}

bool frameMatchesRgb(const std::shared_ptr<ImageBuffer> &frame, int r, int g, int b)
{
    return frame && frame->channels == 3 && frame->data.size() >= 3
        && channelNear(frame->data[0], r)
        && channelNear(frame->data[1], g)
        && channelNear(frame->data[2], b);
}
}

void TestVideoInputService::testOpenCvFilePlaybackProducesRgbFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeTestVideoOrEmpty(dir);
    if (path.isEmpty()) {
        QSKIP("OpenCV VideoWriter could not create the test AVI in this environment.");
    }

    VideoInputService service;
    std::vector<std::shared_ptr<ImageBuffer>> frames;
    connect(&service, &VideoInputService::frameReady, this,
            [&frames](std::shared_ptr<ImageBuffer> frame) {
                frames.push_back(std::move(frame));
            });

    QVERIFY(service.open(path));
    QVERIFY(service.isOpen());
    QCOMPARE(service.videoWidth(), 4);
    QCOMPARE(service.videoHeight(), 2);
    QVERIFY(std::abs(service.fps() - 5.0) <= 0.5);
    QVERIFY(service.durationSecs() > 0.0);

    service.stepForward();
    service.stepForward();
    service.stepForward();
    QCOMPARE(frames.size(), std::size_t{3});

    const auto &first = frames.front();
    QVERIFY(first != nullptr);
    QCOMPARE(first->width, 4);
    QCOMPARE(first->height, 2);
    QCOMPARE(first->channels, 3);
    QCOMPARE(first->data.size(), std::size_t{24});
    QVERIFY(frameMatchesRgb(first, 30, 20, 10));

    service.stepForward();
    QCOMPARE(frames.size(), std::size_t{3});
}

void TestVideoInputService::testSeekToStartDeliversFirstFrame()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeTestVideoOrEmpty(dir);
    if (path.isEmpty()) {
        QSKIP("OpenCV VideoWriter could not create the test AVI in this environment.");
    }

    VideoInputService service;
    std::vector<std::shared_ptr<ImageBuffer>> frames;
    connect(&service, &VideoInputService::frameReady, this,
            [&frames](std::shared_ptr<ImageBuffer> frame) {
                frames.push_back(std::move(frame));
            });

    QVERIFY(service.open(path));
    service.stepForward();
    service.stepForward();
    QCOMPARE(frames.size(), std::size_t{2});
    QVERIFY(frameMatchesRgb(frames[0], 30, 20, 10));
    QVERIFY(frameMatchesRgb(frames[1], 60, 50, 40));

    service.seekToSecs(0.0);
    QCOMPARE(frames.size(), std::size_t{3});
    QVERIFY(frameMatchesRgb(frames.back(), 30, 20, 10));
    QVERIFY(service.currentTimeSecs() >= 0.0);
}

void TestVideoInputService::testTimerPlaybackStopsAtEofWhenLoopDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeTestVideoOrEmpty(dir);
    if (path.isEmpty()) {
        QSKIP("OpenCV VideoWriter could not create the test AVI in this environment.");
    }

    VideoInputService service;
    std::vector<std::shared_ptr<ImageBuffer>> frames;
    bool finished = false;
    connect(&service, &VideoInputService::frameReady, this,
            [&frames](std::shared_ptr<ImageBuffer> frame) {
                frames.push_back(std::move(frame));
            });
    connect(&service, &VideoInputService::playbackFinished, this,
            [&finished]() {
                finished = true;
            });

    QVERIFY(service.open(path));
    service.setLoop(false);
    service.setSpeed(10.0);
    service.play();

    QTRY_VERIFY_WITH_TIMEOUT(finished, 1500);
    QVERIFY(!service.isPlaying());
    QCOMPARE(frames.size(), std::size_t{3});
}

void TestVideoInputService::testTimerPlaybackLoopsWhenEnabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeTestVideoOrEmpty(dir);
    if (path.isEmpty()) {
        QSKIP("OpenCV VideoWriter could not create the test AVI in this environment.");
    }

    VideoInputService service;
    std::vector<std::shared_ptr<ImageBuffer>> frames;
    connect(&service, &VideoInputService::frameReady, this,
            [&frames](std::shared_ptr<ImageBuffer> frame) {
                frames.push_back(std::move(frame));
            });

    QVERIFY(service.open(path));
    service.setLoop(true);
    service.setSpeed(10.0);
    service.play();

    QTRY_VERIFY_WITH_TIMEOUT(frames.size() >= std::size_t{4}, 2000);
    service.pause();
    QVERIFY(!service.isPlaying());
    QVERIFY(frameMatchesRgb(frames[0], 30, 20, 10));
    QVERIFY(frameMatchesRgb(frames[1], 60, 50, 40));
    QVERIFY(frameMatchesRgb(frames[2], 90, 80, 70));
    QVERIFY(frameMatchesRgb(frames[3], 30, 20, 10));
}

void TestVideoInputService::testInvalidSpeedFallsBackToNormalRate()
{
    VideoInputService service;

    service.setSpeed(0.0);
    QCOMPARE(service.speed(), 1.0);

    service.setSpeed(-2.0);
    QCOMPARE(service.speed(), 1.0);
}

void TestVideoInputService::testMissingFileReportsError()
{
    VideoInputService service;
    QStringList errors;
    connect(&service, &VideoInputService::errorOccurred, this,
            [&errors](const QString &message) {
                errors.push_back(message);
            });

    QVERIFY(!service.open(QStringLiteral("/tmp/qt_ui_opencv_missing_video.avi")));
    QVERIFY(!service.isOpen());
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.front().contains(QStringLiteral("Failed to open video file")));
}

void TestVideoInputService::testOpenCvStreamProducesLatestFramesAndStatus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeTestVideoOrEmpty(dir);
    if (path.isEmpty()) {
        QSKIP("OpenCV VideoWriter could not create the test AVI in this environment.");
    }

    VideoInputService service;
    std::vector<std::shared_ptr<ImageBuffer>> frames;
    QSignalSpy statusSpy(&service, &VideoInputService::streamStatusChanged);
    connect(&service, &VideoInputService::frameReady, this,
            [&frames](std::shared_ptr<ImageBuffer> frame) {
                frames.push_back(std::move(frame));
            });

    QVERIFY(service.openStream(path));
    QVERIFY(service.isOpen());
    QVERIFY(service.isStream());
    QCOMPARE(service.streamStatus(), VideoInputService::StreamStatus::Connected);
    QCOMPARE(service.videoWidth(), 4);
    QCOMPARE(service.videoHeight(), 2);

    QTRY_VERIFY_WITH_TIMEOUT(!frames.empty(), 1500);
    const auto &firstDelivered = frames.front();
    QVERIFY(firstDelivered != nullptr);
    QCOMPARE(firstDelivered->width, 4);
    QCOMPARE(firstDelivered->height, 2);
    QCOMPARE(firstDelivered->channels, 3);

    service.closeStream();
    QCOMPARE(service.streamStatus(), VideoInputService::StreamStatus::Disconnected);
    QVERIFY(statusSpy.size() >= 2);
}

void TestVideoInputService::testMissingStreamReportsError()
{
    VideoInputService service;
    QStringList errors;
    connect(&service, &VideoInputService::errorOccurred, this,
            [&errors](const QString &message) {
                errors.push_back(message);
            });

    QVERIFY(!service.openStream(QStringLiteral("/tmp/qt_ui_opencv_missing_stream.avi")));
    QVERIFY(!service.isOpen());
    QVERIFY(!service.isStream());
    QCOMPARE(service.streamStatus(), VideoInputService::StreamStatus::Disconnected);
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.front().contains(QStringLiteral("Failed to open stream")));
}

QTEST_MAIN(TestVideoInputService)
#include "tst_VideoInputService.moc"
