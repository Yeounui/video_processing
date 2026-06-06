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
    void testMissingFileReportsError();
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

bool channelNear(int actual, int expected)
{
    return std::abs(actual - expected) <= 30;
}
}

void TestVideoInputService::testOpenCvFilePlaybackProducesRgbFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("source.avi"));
    if (!writeTestVideo(path)) {
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
    QVERIFY(channelNear(first->data[0], 30));
    QVERIFY(channelNear(first->data[1], 20));
    QVERIFY(channelNear(first->data[2], 10));

    service.stepForward();
    QCOMPARE(frames.size(), std::size_t{3});
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

QTEST_MAIN(TestVideoInputService)
#include "tst_VideoInputService.moc"
