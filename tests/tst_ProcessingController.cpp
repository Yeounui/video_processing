#include <QtTest>
#include "ProcessingController.h"
#include <QImage>
#include <QTemporaryDir>

class TestProcessingController : public QObject {
    Q_OBJECT
private slots:
    void testOpenImageFromFileUrl();
};

void TestProcessingController::testOpenImageFromFileUrl() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("source.png"));

    QImage source(4, 2, QImage::Format_RGB888);
    source.fill(QColor(64, 128, 192));
    QVERIFY(source.save(path));

    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(path));

    QVERIFY(controller.hasImage());
    QVERIFY(controller.canSave());
    QCOMPARE(controller.imageWidth(), 4);
    QCOMPARE(controller.imageHeight(), 2);
    QCOMPARE(controller.sourceFileName(), QStringLiteral("source.png"));
    QVERIFY(controller.inImage() != nullptr);
    QVERIFY(controller.outImage() != nullptr);
    QCOMPARE(controller.inImage()->channels, 3);
    QCOMPARE(controller.outImage()->channels, 3);
}

QTEST_MAIN(TestProcessingController)
#include "tst_ProcessingController.moc"
