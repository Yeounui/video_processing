#include <QtTest>
#include "ImageIoService.h"
#include <QImage>
#include <QTemporaryDir>

class TestImageIoService : public QObject {
    Q_OBJECT
private slots:
    void testLoadWebp();
    void testLoadTransparentPngPreservesAlpha();
};

void TestImageIoService::testLoadWebp() {
    auto img = ImageIoService::load(QStringLiteral(TEST_IMAGE_PATH));
    QVERIFY(img != nullptr);
    QCOMPARE(img->width, 1920);
    QCOMPARE(img->height, 1199);
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
