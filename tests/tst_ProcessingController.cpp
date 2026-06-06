#include <QtTest>
#include "GpuEffectPipeline.h"
#include "ProcessingController.h"
#include <QImage>
#include <QTemporaryDir>

class TestProcessingController : public QObject {
    Q_OBJECT
private slots:
    void testOpenImageFromFileUrl();
    void testOpenSourceRoutesImageByExtension();
    void testHistoryLabelsTrackApplyUndoRedoReset();
    void testGpuFusedStackSupport();
    void testMedianGpuAndHybridSuffixSupport();
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

void TestProcessingController::testOpenSourceRoutesImageByExtension() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("source.PNG"));

    QImage source(3, 3, QImage::Format_RGB888);
    source.fill(QColor(10, 20, 30));
    QVERIFY(source.save(path));

    ProcessingController controller;
    controller.openSource(QUrl::fromLocalFile(path));

    QVERIFY(controller.hasImage());
    QVERIFY(controller.canSave());
    QCOMPARE(controller.sourceFileName(), QStringLiteral("source.PNG"));
    QVERIFY(!controller.isVideoSource());
}

void TestProcessingController::testHistoryLabelsTrackApplyUndoRedoReset() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("source.png"));

    QImage source(5, 5, QImage::Format_RGB888);
    source.fill(QColor(64, 128, 192));
    QVERIFY(source.save(path));

    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(path));
    QVERIFY(controller.historyLabels().isEmpty());
    QCOMPARE(controller.historyIndex(), -1);

    QVariantMap params;
    params.insert(QStringLiteral("kernel"), QStringLiteral("3"));
    params.insert(QStringLiteral("sigma"), 1.0);
    controller.applyAlgorithm(13, params);

    QCOMPARE(controller.historyLabels(), QStringList({QStringLiteral("Gaussian Blur")}));
    QCOMPARE(controller.historyIndex(), 0);
    QVERIFY(controller.canUndo());

    controller.undo();
    QCOMPARE(controller.historyLabels(), QStringList({QStringLiteral("Gaussian Blur")}));
    QCOMPARE(controller.historyIndex(), -1);
    QVERIFY(controller.canRedo());

    controller.redo();
    QCOMPARE(controller.historyIndex(), 0);

    controller.reset();
    QVERIFY(controller.historyLabels().isEmpty());
    QCOMPARE(controller.historyIndex(), -1);
}

void TestProcessingController::testGpuFusedStackSupport() {
    std::vector<GpuEffectCommand> fused = {
        {7, QVariantMap{{QStringLiteral("mode"), QStringLiteral("H")}}},
        {1, QVariantMap{{QStringLiteral("delta"), 12}}},
        {26, QVariantMap{}},
    };
    QVERIFY(GpuEffectPipeline::supportsFusedStack(fused));

    std::vector<GpuEffectCommand> multiPass = {
        {7, QVariantMap{{QStringLiteral("mode"), QStringLiteral("H")}}},
        {11, QVariantMap{}},
    };
    QVERIFY(GpuEffectPipeline::supportsAlgorithm(11));
    QVERIFY(!GpuEffectPipeline::supportsFusedStack(multiPass));

    std::vector<GpuEffectCommand> cpuOnly = {
        {8, QVariantMap{{QStringLiteral("degree"), 30.0}}},
    };
    QVERIFY(!GpuEffectPipeline::supportsAlgorithm(8));
    QVERIFY(!GpuEffectPipeline::supportsFusedStack(cpuOnly));
}

void TestProcessingController::testMedianGpuAndHybridSuffixSupport() {
    QVERIFY(GpuEffectPipeline::supportsAlgorithm(25));
    std::vector<GpuEffectCommand> medianStack = {{25, QVariantMap{}}};
    QVERIFY(GpuEffectPipeline::supportsAlgorithm(medianStack[0].algorithmId));
    QVERIFY(!GpuEffectPipeline::supportsFusedStack(medianStack));
}

QTEST_MAIN(TestProcessingController)
#include "tst_ProcessingController.moc"
