#include <QtTest>
#include "ProcessingController.h"

class TestProcessingController : public QObject {
    Q_OBJECT
private slots:
    void testOpenImageFromFileUrl();
};

void TestProcessingController::testOpenImageFromFileUrl() {
    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(QStringLiteral(TEST_IMAGE_PATH)));

    QVERIFY(controller.hasImage());
    QVERIFY(controller.canSave());
    QCOMPARE(controller.imageWidth(), 1920);
    QCOMPARE(controller.imageHeight(), 1199);
    QCOMPARE(controller.sourceFileName(), QStringLiteral("desktop.webp"));
    QVERIFY(controller.inImage() != nullptr);
    QVERIFY(controller.outImage() != nullptr);
    QCOMPARE(controller.inImage()->channels, 3);
    QCOMPARE(controller.outImage()->channels, 3);
}

QTEST_MAIN(TestProcessingController)
#include "tst_ProcessingController.moc"
