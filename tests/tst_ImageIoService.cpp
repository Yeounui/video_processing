#include <QtTest>
#include "ImageIoService.h"
#include <QImage>
#include <QTemporaryDir>

class TestImageIoService : public QObject {
    Q_OBJECT
private slots:
    void testLoadRgbImage();
    void testLoadTransparentPngPreservesAlpha();
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
}

QTEST_MAIN(TestImageIoService)
#include "tst_ImageIoService.moc"
