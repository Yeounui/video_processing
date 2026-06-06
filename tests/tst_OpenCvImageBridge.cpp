#include <QtTest>

#include "OpenCvImageBridge.h"

class TestOpenCvImageBridge : public QObject {
    Q_OBJECT

private slots:
    void testRejectsInvalidLayouts();
    void testCreatesMutableRgbAndRgbaViews();
    void testCopiesRgbMatToImageBuffer();
    void testConvertsBgrAndBgraToBoundaryOrder();
    void testCopiesNonContinuousMat();
    void testCopiesImageBufferToBgrMat();
    void testHandlesEmptyBuffers();
};

void TestOpenCvImageBridge::testRejectsInvalidLayouts()
{
    ImageBuffer invalidChannels;
    invalidChannels.width = 1;
    invalidChannels.height = 1;
    invalidChannels.channels = 1;
    invalidChannels.data = {42};
    QVERIFY(!OpenCvImageBridge::isSupportedImageBuffer(invalidChannels));
    QVERIFY(OpenCvImageBridge::mutableMatView(invalidChannels).empty());

    ImageBuffer mismatchedSize;
    mismatchedSize.width = 2;
    mismatchedSize.height = 1;
    mismatchedSize.channels = 3;
    mismatchedSize.data = {1, 2, 3};
    QVERIFY(!OpenCvImageBridge::isSupportedImageBuffer(mismatchedSize));

    cv::Mat unsupportedDepth(1, 1, CV_16UC3);
    ImageBuffer dst;
    dst.width = 9;
    dst.height = 9;
    QVERIFY(!OpenCvImageBridge::copyMatToImageBuffer(unsupportedDepth, dst));
    QCOMPARE(dst.width, 9);
    QCOMPARE(dst.height, 9);
}

void TestOpenCvImageBridge::testCreatesMutableRgbAndRgbaViews()
{
    ImageBuffer rgb;
    rgb.width = 2;
    rgb.height = 1;
    rgb.channels = 3;
    rgb.data = {1, 2, 3, 4, 5, 6};

    cv::Mat rgbView = OpenCvImageBridge::mutableMatView(rgb);
    QCOMPARE(rgbView.type(), CV_8UC3);
    QCOMPARE(rgbView.cols, 2);
    QCOMPARE(rgbView.rows, 1);
    QCOMPARE(rgbView.data, rgb.data.data());
    rgbView.at<cv::Vec3b>(0, 1) = cv::Vec3b(9, 8, 7);
    QCOMPARE(rgb.data[3], (uint8_t)9);
    QCOMPARE(rgb.data[4], (uint8_t)8);
    QCOMPARE(rgb.data[5], (uint8_t)7);

    ImageBuffer rgba;
    rgba.width = 1;
    rgba.height = 1;
    rgba.channels = 4;
    rgba.data = {10, 20, 30, 40};
    cv::Mat rgbaView = OpenCvImageBridge::mutableMatView(rgba);
    QCOMPARE(rgbaView.type(), CV_8UC4);
    QCOMPARE(rgbaView.data, rgba.data.data());
}

void TestOpenCvImageBridge::testCopiesRgbMatToImageBuffer()
{
    cv::Mat mat(1, 2, CV_8UC3);
    mat.at<cv::Vec3b>(0, 0) = cv::Vec3b(10, 20, 30);
    mat.at<cv::Vec3b>(0, 1) = cv::Vec3b(40, 50, 60);

    ImageBuffer dst;
    QVERIFY(OpenCvImageBridge::copyMatToImageBuffer(mat, dst));
    QCOMPARE(dst.width, 2);
    QCOMPARE(dst.height, 1);
    QCOMPARE(dst.channels, 3);
    QCOMPARE(dst.data, std::vector<uint8_t>({10, 20, 30, 40, 50, 60}));
}

void TestOpenCvImageBridge::testConvertsBgrAndBgraToBoundaryOrder()
{
    cv::Mat bgr(1, 1, CV_8UC3);
    bgr.at<cv::Vec3b>(0, 0) = cv::Vec3b(30, 20, 10);

    ImageBuffer rgb;
    QVERIFY(OpenCvImageBridge::copyMatToImageBuffer(
        bgr, rgb, OpenCvImageBridge::ColorOrder::Bgr));
    QCOMPARE(rgb.data, std::vector<uint8_t>({10, 20, 30}));

    cv::Mat bgra(1, 1, CV_8UC4);
    bgra.at<cv::Vec4b>(0, 0) = cv::Vec4b(30, 20, 10, 40);

    ImageBuffer rgba;
    QVERIFY(OpenCvImageBridge::copyMatToImageBuffer(
        bgra, rgba, OpenCvImageBridge::ColorOrder::Bgr));
    QCOMPARE(rgba.data, std::vector<uint8_t>({10, 20, 30, 40}));
}

void TestOpenCvImageBridge::testCopiesNonContinuousMat()
{
    cv::Mat base(2, 3, CV_8UC3);
    for (int y = 0; y < base.rows; ++y)
        for (int x = 0; x < base.cols; ++x)
            base.at<cv::Vec3b>(y, x) = cv::Vec3b(x, y, x + y);

    cv::Mat roi = base(cv::Rect(1, 0, 2, 2));
    QVERIFY(!roi.isContinuous());

    ImageBuffer dst;
    QVERIFY(OpenCvImageBridge::copyMatToImageBuffer(roi, dst));
    QCOMPARE(dst.width, 2);
    QCOMPARE(dst.height, 2);
    QCOMPARE(dst.channels, 3);
    QCOMPARE(dst.data, std::vector<uint8_t>({1, 0, 1, 2, 0, 2, 1, 1, 2, 2, 1, 3}));
}

void TestOpenCvImageBridge::testCopiesImageBufferToBgrMat()
{
    ImageBuffer src;
    src.width = 1;
    src.height = 1;
    src.channels = 4;
    src.data = {10, 20, 30, 40};

    cv::Mat mat = OpenCvImageBridge::copyImageBufferToMat(
        src, OpenCvImageBridge::ColorOrder::Bgr);
    QCOMPARE(mat.type(), CV_8UC4);
    const cv::Vec4b px = mat.at<cv::Vec4b>(0, 0);
    QCOMPARE(px[0], (uint8_t)30);
    QCOMPARE(px[1], (uint8_t)20);
    QCOMPARE(px[2], (uint8_t)10);
    QCOMPARE(px[3], (uint8_t)40);
}

void TestOpenCvImageBridge::testHandlesEmptyBuffers()
{
    ImageBuffer empty;
    empty.width = 0;
    empty.height = 0;
    empty.channels = 3;
    QVERIFY(OpenCvImageBridge::isSupportedImageBuffer(empty));
    QVERIFY(OpenCvImageBridge::mutableMatView(empty).empty());

    cv::Mat emptyMat(0, 0, CV_8UC4);
    ImageBuffer dst;
    QVERIFY(OpenCvImageBridge::copyMatToImageBuffer(emptyMat, dst));
    QCOMPARE(dst.width, 0);
    QCOMPARE(dst.height, 0);
    QCOMPARE(dst.channels, 4);
    QVERIFY(dst.data.empty());
}

QTEST_MAIN(TestOpenCvImageBridge)
#include "tst_OpenCvImageBridge.moc"
