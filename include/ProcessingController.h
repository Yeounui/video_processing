#pragma once

#include "EditCommand.h"
#include "ImageBuffer.h"
#include <deque>
#include <vector>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

class AlgorithmModel;
class VideoInputService;

class ProcessingController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY hasImageChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY canSaveChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(AlgorithmModel* algorithmModel READ algorithmModel CONSTANT)
    Q_PROPERTY(QString sourceFileName READ sourceFileName NOTIFY sourceChanged)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY sourceChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY sourceChanged)
    // Video properties
    Q_PROPERTY(bool isVideoSource READ isVideoSource NOTIFY sourceChanged)
    Q_PROPERTY(bool videoPlaying READ videoPlaying NOTIFY videoPlayingChanged)
    Q_PROPERTY(double videoDuration READ videoDuration NOTIFY sourceChanged)
    Q_PROPERTY(double videoPosition READ videoPosition NOTIFY videoPositionChanged)
    Q_PROPERTY(int effectStackSize READ effectStackSize NOTIFY effectStackChanged)

public:
    explicit ProcessingController(QObject *parent = nullptr);
    ~ProcessingController() override;

    // Q_PROPERTY read accessors
    bool hasImage() const;
    bool canSave() const;
    bool canUndo() const;
    bool canRedo() const;
    AlgorithmModel *algorithmModel() const;
    QString sourceFileName() const;
    int imageWidth() const;
    int imageHeight() const;
    bool isVideoSource() const;
    bool videoPlaying() const;
    double videoDuration() const;
    double videoPosition() const;
    int effectStackSize() const;

    // Public (non-QML) accessors
    std::shared_ptr<ImageBuffer> inImage() const;
    std::shared_ptr<ImageBuffer> outImage() const;
    quint64 inImageVersion() const;
    quint64 outImageVersion() const;
    void setOutImageDirect(std::shared_ptr<ImageBuffer> img);

    // GPU state accessors
    bool gpuApplyPending() const { return gpuApplyPending_; }
    void commitGpuResult(std::shared_ptr<ImageBuffer> result);
    void cancelGpuApply();

    // Public slots (Q_INVOKABLE)
    Q_INVOKABLE void openImage(const QUrl &url);
    Q_INVOKABLE void saveImage(const QUrl &url);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void applyAlgorithm(int algorithmId, const QVariantMap &params);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // Video control
    Q_INVOKABLE void openVideo(const QUrl &url);
    Q_INVOKABLE void playVideo();
    Q_INVOKABLE void pauseVideo();
    Q_INVOKABLE void stepForwardVideo();
    Q_INVOKABLE void stepBackwardVideo();
    Q_INVOKABLE void seekVideo(double secs);
    Q_INVOKABLE void setVideoLoop(bool loop);
    Q_INVOKABLE void setVideoSpeed(double speed);

    // Effect stack (video/stream mode, max 3 entries)
    Q_INVOKABLE bool appendEffect(int algorithmId, const QVariantMap &params);
    Q_INVOKABLE void removeEffect(int index);
    Q_INVOKABLE void clearEffectStack();

signals:
    void hasImageChanged();
    void canSaveChanged();
    void historyChanged();
    void sourceChanged();
    void imageChanged();
    void errorOccurred(const QString &message);
    void pendingGpuApply(std::shared_ptr<ImageBuffer> src, int algorithmId, QVariantMap params);
    // Video signals
    void videoPlayingChanged();
    void videoPositionChanged(double secs);
    void effectStackChanged();

private slots:
    void onVideoFrame(std::shared_ptr<ImageBuffer> frame);
    void onVideoPosition(double secs);
    void onVideoPlaybackFinished();

private:
    void clearHistory();
    void pushCommand(std::unique_ptr<EditCommand> command);
    // Apply effectStack_ to src sequentially; returns result or src if stack is empty.
    std::shared_ptr<ImageBuffer> applyEffectStack(std::shared_ptr<ImageBuffer> src);

    std::shared_ptr<ImageBuffer> inImage_;
    std::shared_ptr<ImageBuffer> outImage_;
    quint64 inImageVersion_ = 0;
    quint64 outImageVersion_ = 0;
    SourceType sourceType_ = SourceType::SOURCE_NONE;
    QString sourceFileName_;
    std::unique_ptr<AlgorithmModel> algorithmModel_;
    std::deque<std::unique_ptr<EditCommand>> history_;
    int historyIndex_ = -1;
    std::size_t historyBytes_ = 0;

    // GPU effect state
    bool gpuApplyPending_ = false;
    std::shared_ptr<ImageBuffer> gpuPrevOut_;

    // Video
    VideoInputService *videoService_ = nullptr;
    double videoPosition_ = 0.0;

    // Effect stack (video/stream mode only; max 3)
    struct EffectEntry { int algorithmId; QVariantMap params; };
    static constexpr int MaxEffectStack = 3;
    std::vector<EffectEntry> effectStack_;
};
