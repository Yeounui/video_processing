#include <QtTest>
#include "GpuEffectPipeline.h"
#include "ProcessingBackend.h"
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
    void testProcessingBackendPlansAcceleratedSuffix();
    void testMedianGpuAndHybridSuffixSupport();
    void testCpuEffectStackComputesStatisticsPerSource();
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

void TestProcessingController::testProcessingBackendPlansAcceleratedSuffix() {
    std::vector<ProcessingBackend::Effect> rotateThenGpu = {
        {8, QVariantMap{{QStringLiteral("degree"), 30.0}}},
        {14, QVariantMap{{QStringLiteral("alpha"), 1.0}}},
        {25, QVariantMap{}},
    };
    const auto rotatePlan = ProcessingBackend::planVideoStack(rotateThenGpu);
    QCOMPARE(rotatePlan.cpuPrefixCount, 1);
    QCOMPARE(rotatePlan.acceleratedSuffix.size(), std::size_t{2});
    QVERIFY(!rotatePlan.suffixCanFuse);

    std::vector<ProcessingBackend::Effect> statsThenGpu = {
        {5, QVariantMap{}},
        {1, QVariantMap{{QStringLiteral("delta"), 12}}},
    };
    const auto statsPlan = ProcessingBackend::planVideoStack(statsThenGpu);
    QCOMPARE(statsPlan.cpuPrefixCount, 1);
    QCOMPARE(statsPlan.acceleratedSuffix.size(), std::size_t{1});
    QCOMPARE(statsPlan.acceleratedSuffix.front().algorithmId, 1);
    QVERIFY(ProcessingBackend::requiresCpuStatistics(5));
}

void TestProcessingController::testMedianGpuAndHybridSuffixSupport() {
    QVERIFY(GpuEffectPipeline::supportsAlgorithm(25));
    std::vector<GpuEffectCommand> medianStack = {{25, QVariantMap{}}};
    QVERIFY(GpuEffectPipeline::supportsAlgorithm(medianStack[0].algorithmId));
    QVERIFY(!GpuEffectPipeline::supportsFusedStack(medianStack));
}

void TestProcessingController::testCpuEffectStackComputesStatisticsPerSource() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("stats_source.png"));

    QImage source(2, 1, QImage::Format_RGB888);
    source.setPixelColor(0, 0, QColor(140, 140, 140));
    source.setPixelColor(1, 0, QColor(180, 180, 180));
    QVERIFY(source.save(path));

    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(path));
    QVERIFY(controller.hasImage());

    QVERIFY(controller.appendEffect(5, QVariantMap{}));

    const auto out = controller.outImage();
    QVERIFY(out != nullptr);
    QCOMPARE(out->width, 2);
    QCOMPARE(out->height, 1);
    QCOMPARE(out->channels, 3);
    QVERIFY(out->data.size() >= 6);
    QCOMPARE(out->data[0], uint8_t{0});
    QCOMPARE(out->data[1], uint8_t{0});
    QCOMPARE(out->data[2], uint8_t{0});
    QCOMPARE(out->data[3], uint8_t{255});
    QCOMPARE(out->data[4], uint8_t{255});
    QCOMPARE(out->data[5], uint8_t{255});
}

QTEST_MAIN(TestProcessingController)
#include "tst_ProcessingController.moc"
