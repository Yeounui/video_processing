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
    void testProcessingBackendCpuReferenceDisablesAcceleration();
    void testMedianGpuAndHybridSuffixSupport();
    void testCpuEffectStackComputesStatisticsPerSource();
    void testCpuReferenceBackendBypassesStaticGpu();
    void testGpuFailureFallsBackToCpuReference();
    void testStaleGpuResultDoesNotOverwriteChangedSource();
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

void TestProcessingController::testProcessingBackendCpuReferenceDisablesAcceleration() {
    std::vector<ProcessingBackend::Effect> gpuSupported = {
        {1, QVariantMap{{QStringLiteral("delta"), 12}}},
        {7, QVariantMap{{QStringLiteral("mode"), QStringLiteral("H")}}},
        {25, QVariantMap{}},
    };

    const auto cpuPlan = ProcessingBackend::planVideoStack(
        gpuSupported, ProcessingBackend::Kind::CpuReference);
    QCOMPARE(cpuPlan.cpuPrefixCount, 3);
    QVERIFY(cpuPlan.acceleratedSuffix.empty());
    QVERIFY(!cpuPlan.suffixCanFuse);
    QVERIFY(!ProcessingBackend::supportsAcceleratedAlgorithm(
        1, ProcessingBackend::Kind::CpuReference));
    QVERIFY(!ProcessingBackend::supportsFusedStack(
        gpuSupported, ProcessingBackend::Kind::CpuReference));
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

void TestProcessingController::testCpuReferenceBackendBypassesStaticGpu() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("cpu_only.png"));

    QImage source(1, 1, QImage::Format_RGB888);
    source.setPixelColor(0, 0, QColor(10, 20, 30));
    QVERIFY(source.save(path));

    ProcessingController controller;
    QCOMPARE(controller.acceleratedBackendKind(), ProcessingBackend::Kind::OpenGl);
    controller.setAcceleratedBackendKind(ProcessingBackend::Kind::CpuReference);
    QCOMPARE(controller.acceleratedBackendKind(), ProcessingBackend::Kind::CpuReference);
    controller.openImage(QUrl::fromLocalFile(path));
    QVERIFY(controller.hasImage());

    controller.applyAlgorithm(1, QVariantMap{{QStringLiteral("delta"), 15}});
    QVERIFY(!controller.gpuApplyPending());

    const auto out = controller.outImage();
    QVERIFY(out != nullptr);
    QCOMPARE(out->width, 1);
    QCOMPARE(out->height, 1);
    QCOMPARE(out->channels, 3);
    QVERIFY(out->data.size() >= 3);
    QCOMPARE(out->data[0], uint8_t{25});
    QCOMPARE(out->data[1], uint8_t{35});
    QCOMPARE(out->data[2], uint8_t{45});
    QCOMPARE(controller.historyLabels(), QStringList({QStringLiteral("Brightness")}));
    QCOMPARE(controller.historyIndex(), 0);
}

void TestProcessingController::testGpuFailureFallsBackToCpuReference() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("gpu_fallback.png"));

    QImage source(1, 1, QImage::Format_RGB888);
    source.setPixelColor(0, 0, QColor(10, 20, 30));
    QVERIFY(source.save(path));

    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(path));
    QVERIFY(controller.hasImage());

    controller.applyAlgorithm(1, QVariantMap{{QStringLiteral("delta"), 15}});
    QVERIFY(controller.gpuApplyPending());

    controller.cancelGpuApply();
    QVERIFY(!controller.gpuApplyPending());

    const auto out = controller.outImage();
    QVERIFY(out != nullptr);
    QCOMPARE(out->width, 1);
    QCOMPARE(out->height, 1);
    QCOMPARE(out->channels, 3);
    QVERIFY(out->data.size() >= 3);
    QCOMPARE(out->data[0], uint8_t{25});
    QCOMPARE(out->data[1], uint8_t{35});
    QCOMPARE(out->data[2], uint8_t{45});
    QCOMPARE(controller.historyLabels(), QStringList({QStringLiteral("Brightness")}));
    QCOMPARE(controller.historyIndex(), 0);
}

void TestProcessingController::testStaleGpuResultDoesNotOverwriteChangedSource() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString firstPath = dir.filePath(QStringLiteral("first.png"));
    const QString secondPath = dir.filePath(QStringLiteral("second.png"));

    QImage first(1, 1, QImage::Format_RGB888);
    first.setPixelColor(0, 0, QColor(10, 20, 30));
    QVERIFY(first.save(firstPath));

    QImage second(1, 1, QImage::Format_RGB888);
    second.setPixelColor(0, 0, QColor(70, 80, 90));
    QVERIFY(second.save(secondPath));

    ProcessingController controller;
    controller.openImage(QUrl::fromLocalFile(firstPath));
    QVERIFY(controller.hasImage());

    controller.applyAlgorithm(1, QVariantMap{{QStringLiteral("delta"), 120}});
    QVERIFY(controller.gpuApplyPending());

    controller.openImage(QUrl::fromLocalFile(secondPath));
    QVERIFY(!controller.gpuApplyPending());

    auto staleResult = std::make_shared<ImageBuffer>();
    staleResult->width = 1;
    staleResult->height = 1;
    staleResult->channels = 3;
    staleResult->data = {200, 210, 220};
    controller.commitGpuResult(staleResult);

    const auto out = controller.outImage();
    QVERIFY(out != nullptr);
    QCOMPARE(out->width, 1);
    QCOMPARE(out->height, 1);
    QCOMPARE(out->channels, 3);
    QVERIFY(out->data.size() >= 3);
    QCOMPARE(out->data[0], uint8_t{70});
    QCOMPARE(out->data[1], uint8_t{80});
    QCOMPARE(out->data[2], uint8_t{90});
    QVERIFY(controller.historyLabels().isEmpty());
    QCOMPARE(controller.historyIndex(), -1);
}

QTEST_MAIN(TestProcessingController)
#include "tst_ProcessingController.moc"
