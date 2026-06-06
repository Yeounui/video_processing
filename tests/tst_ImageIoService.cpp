#include <QtTest>
#include "ImageIoService.h"
#include <QImage>
#include <QTemporaryDir>

class TestImageIoService : public QObject {
    Q_OBJECT
private slots:
    void testLoadRgbImage();
    void testLoadTransparentPngPreservesAlpha();
    void testSaveAndLoadRgbPngPreservesColorOrder();
    void testSaveAndLoadRgbaPngPreservesAlpha();
    void testLoadMissingFileReturnsNull();
    void testSaveInvalidBufferReturnsFalse();
};

void TestImageIoService::testLoadRgbImage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("rgb.png"));

    QImage source(4, 2, QImage::Format_RGB888);
    source.fill(QColor(10, 20, 30));
    QVERIFY(source.save(path));

    auto img = ImageIoService::load(path);
    QVERIFY(img != nullptr);
    QCOMPARE(img->width, 4);
    QCOMPARE(img->height, 2);
    QCOMPARE(img->channels, 3);
    QCOMPARE((int)img->data.size(), img->width * img->height * img->channels);
    QCOMPARE(img->data[0], (uint8_t)10);
    QCOMPARE(img->data[1], (uint8_t)20);
    QCOMPARE(img->data[2], (uint8_t)30);
}

void TestImageIoService::testLoadTransparentPngPreservesAlpha() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("transparent.png"));

    QImage source(3, 3, QImage::Format_RGBA8888);
    source.fill(Qt::transparent);
    source.setPixelColor(1, 1, QColor(255, 0, 0, 255));
    QVERIFY(source.save(path));

    auto img = ImageIoService::load(path);
    QVERIFY(img != nullptr);
    QCOMPARE(img->width, 3);
    QCOMPARE(img->height, 3);
    QCOMPARE(img->channels, 4);
    QCOMPARE(img->data[(0 * img->width + 0) * img->channels + 3], (uint8_t)0);
    QCOMPARE(img->data[(1 * img->width + 1) * img->channels + 3], (uint8_t)255);
    QCOMPARE(img->data[(1 * img->width + 1) * img->channels + 0], (uint8_t)255);
    QCOMPARE(img->data[(1 * img->width + 1) * img->channels + 1], (uint8_t)0);
    QCOMPARE(img->data[(1 * img->width + 1) * img->channels + 2], (uint8_t)0);
}

void TestImageIoService::testSaveAndLoadRgbPngPreservesColorOrder() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("roundtrip_rgb.png"));

    ImageBuffer source;
    source.width = 2;
    source.height = 1;
    source.channels = 3;
    source.data = {
        255, 0, 0,
        0, 255, 0,
    };

    QVERIFY(ImageIoService::save(source, path));
    auto img = ImageIoService::load(path);
    QVERIFY(img != nullptr);
    QCOMPARE(img->width, source.width);
    QCOMPARE(img->height, source.height);
    QCOMPARE(img->channels, source.channels);
    QCOMPARE(img->data.size(), source.data.size());
    for (size_t i = 0; i < source.data.size(); ++i)
        QCOMPARE(img->data[i], source.data[i]);
}

void TestImageIoService::testSaveAndLoadRgbaPngPreservesAlpha() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("roundtrip_rgba.png"));

    ImageBuffer source;
    source.width = 2;
    source.height = 1;
    source.channels = 4;
    source.data = {
        10, 20, 30, 0,
        40, 50, 60, 200,
    };

    QVERIFY(ImageIoService::save(source, path));
    auto img = ImageIoService::load(path);
    QVERIFY(img != nullptr);
    QCOMPARE(img->width, source.width);
    QCOMPARE(img->height, source.height);
    QCOMPARE(img->channels, source.channels);
    QCOMPARE(img->data.size(), source.data.size());
    for (size_t i = 0; i < source.data.size(); ++i)
        QCOMPARE(img->data[i], source.data[i]);
}

void TestImageIoService::testLoadMissingFileReturnsNull() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto img = ImageIoService::load(dir.filePath(QStringLiteral("missing.png")));
    QVERIFY(img == nullptr);
}

void TestImageIoService::testSaveInvalidBufferReturnsFalse() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("invalid.png"));

    ImageBuffer source;
    source.width = 2;
    source.height = 2;
    source.channels = 3;
    source.data = {1, 2, 3};

    QVERIFY(!ImageIoService::save(source, path));
}

QTEST_MAIN(TestImageIoService)
#include "tst_ImageIoService.moc"
