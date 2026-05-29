#include "ProcessingController.h"
#include "AlgorithmModel.h"
#include "ImageProcessorCore.h"
#include "ImageIoService.h"
#include "GpuEffectPipeline.h"
#include "VideoInputService.h"
#include <QFileInfo>
#include <QUrl>
#include <utility>

namespace {
constexpr std::size_t MaxHistoryCount = 20;
constexpr std::size_t MaxHistoryBytes = 512ULL * 1024ULL * 1024ULL;
}

StaticApplyCommand::StaticApplyCommand(ProcessingController *ctrl,
                                       std::shared_ptr<ImageBuffer> prev,
                                       std::shared_ptr<ImageBuffer> next)
    : controller_(ctrl)
    , prev_(std::move(prev))
    , next_(std::move(next))
{
}

void StaticApplyCommand::undo()
{
    if (controller_)
        controller_->setOutImageDirect(prev_);
}

void StaticApplyCommand::redo()
{
    if (controller_)
        controller_->setOutImageDirect(next_);
}

std::size_t StaticApplyCommand::memoryBytes() const
{
    const std::size_t prevBytes = prev_ ? prev_->data.size() : 0;
    const std::size_t nextBytes = next_ ? next_->data.size() : 0;
    return prevBytes + nextBytes;
}

ProcessingController::ProcessingController(QObject *parent)
    : QObject(parent)
    , algorithmModel_(std::make_unique<AlgorithmModel>(this))
{
}

ProcessingController::~ProcessingController() = default;

bool ProcessingController::isVideoSource() const
{
    return sourceType_ == SourceType::SOURCE_VIDEO_FILE;
}

bool ProcessingController::videoPlaying() const
{
    return videoService_ && videoService_->isPlaying();
}

double ProcessingController::videoDuration() const
{
    return videoService_ ? videoService_->durationSecs() : 0.0;
}

double ProcessingController::videoPosition() const
{
    return videoPosition_;
}

int ProcessingController::effectStackSize() const
{
    return static_cast<int>(effectStack_.size());
}

bool ProcessingController::hasImage() const
{
    return inImage_ != nullptr;
}

bool ProcessingController::canSave() const
{
    return hasImage() && sourceType_ == SourceType::SOURCE_IMAGE;
}

bool ProcessingController::canUndo() const
{
    return historyIndex_ >= 0;
}

bool ProcessingController::canRedo() const
{
    return historyIndex_ < static_cast<int>(history_.size()) - 1;
}

AlgorithmModel *ProcessingController::algorithmModel() const
{
    return algorithmModel_.get();
}

QString ProcessingController::sourceFileName() const
{
    return sourceFileName_;
}

int ProcessingController::imageWidth() const
{
    return inImage_ ? inImage_->width : 0;
}

int ProcessingController::imageHeight() const
{
    return inImage_ ? inImage_->height : 0;
}

std::shared_ptr<ImageBuffer> ProcessingController::inImage() const
{
    return inImage_;
}

std::shared_ptr<ImageBuffer> ProcessingController::outImage() const
{
    return outImage_;
}

quint64 ProcessingController::inImageVersion() const
{
    return inImageVersion_;
}

quint64 ProcessingController::outImageVersion() const
{
    return outImageVersion_;
}

void ProcessingController::setOutImageDirect(std::shared_ptr<ImageBuffer> img)
{
    outImage_ = std::move(img);
    ++outImageVersion_;
    emit imageChanged();
    emit canSaveChanged();
}

void ProcessingController::openImage(const QUrl &url)
{
    QString path = url.toLocalFile();
    auto newBuf = ImageIoService::load(path);

    // Error preservation: if load fails, do NOT touch inImage_/outImage_
    if (!newBuf) {
        emit errorOccurred(QString("Failed to load image: %1").arg(path));
        return;
    }

    // Load succeeded, now update state
    inImage_ = newBuf;
    ++inImageVersion_;
    outImage_ = std::make_shared<ImageBuffer>(*newBuf);  // deep copy
    ++outImageVersion_;
    sourceType_ = SourceType::SOURCE_IMAGE;
    sourceFileName_ = QFileInfo(path).fileName();
    clearHistory();

    // Emit all relevant signals
    emit sourceChanged();
    emit hasImageChanged();
    emit canSaveChanged();
    emit imageChanged();
}

void ProcessingController::saveImage(const QUrl &url)
{
    if (!canSave()) {
        emit errorOccurred("Cannot save: no image loaded or source type is not SOURCE_IMAGE");
        return;
    }

    QString path = url.toLocalFile();
    if (!ImageIoService::save(*outImage_, path)) {
        emit errorOccurred(QString("Failed to save image: %1").arg(path));
    }
}

void ProcessingController::openVideo(const QUrl &url)
{
    QString path = url.toLocalFile();

    if (!videoService_) {
        videoService_ = new VideoInputService(this);
        connect(videoService_, &VideoInputService::frameReady, this, &ProcessingController::onVideoFrame);
        connect(videoService_, &VideoInputService::positionChanged, this, &ProcessingController::onVideoPosition);
        connect(videoService_, &VideoInputService::playbackFinished, this, &ProcessingController::onVideoPlaybackFinished);
        connect(videoService_, &VideoInputService::errorOccurred, this, &ProcessingController::errorOccurred);
    } else {
        videoService_->close();
    }

    if (!videoService_->open(path)) {
        return;  // errorOccurred already emitted by videoService_
    }

    clearHistory();
    effectStack_.clear();
    inImage_.reset();
    outImage_.reset();
    ++inImageVersion_;
    ++outImageVersion_;
    sourceType_ = SourceType::SOURCE_VIDEO_FILE;
    sourceFileName_ = QFileInfo(path).fileName();
    videoPosition_ = 0.0;

    emit sourceChanged();
    emit hasImageChanged();
    emit canSaveChanged();
    emit effectStackChanged();

    // Deliver first frame
    videoService_->stepForward();
}

void ProcessingController::reset()
{
    clearHistory();

    if (!inImage_) {
        return;
    }

    // Video mode: clear effect stack only
    if (sourceType_ == SourceType::SOURCE_VIDEO_FILE) {
        clearEffectStack();
        return;
    }

    // D21: Reset semantics: outImage_ restored from inImage_ (deep copy only, do NOT re-read disk)
    outImage_ = std::make_shared<ImageBuffer>(*inImage_);
    ++outImageVersion_;
    emit imageChanged();
}

void ProcessingController::applyAlgorithm(int algorithmId, const QVariantMap &params)
{
    if (!hasImage() || !outImage_ || algorithmId < 1 || algorithmId > 28) {
        emit errorOccurred(QStringLiteral("Cannot apply algorithm: no image loaded or invalid algorithm id"));
        return;
    }

    if (gpuApplyPending_) {
        emit errorOccurred(QStringLiteral("GPU effect in progress"));
        return;
    }

    QVariantMap mutableParams = params;

    // Video mode: append to effect stack instead of static apply
    if (sourceType_ == SourceType::SOURCE_VIDEO_FILE) {
        if (!appendEffect(algorithmId, mutableParams)) {
            emit errorOccurred(QStringLiteral("Effect stack is full (max 3)"));
        }
        return;
    }
    if (algorithmId == 5) {
        mutableParams.insert(QStringLiteral("stat_average"),
                             ImageProcessorCore::computeAverageLuminance(*outImage_));
    } else if (algorithmId == 10) {
        auto [mn, mx] = ImageProcessorCore::computeMinMax(*outImage_);
        mutableParams.insert(QStringLiteral("stat_min"), static_cast<int>(mn));
        mutableParams.insert(QStringLiteral("stat_max"), static_cast<int>(mx));
    } else if (algorithmId == 23) {
        const auto hist = ImageProcessorCore::computeLuminanceHistogram(*outImage_);
        int hmin = 0;
        int hmax = 255;
        for (int i = 0; i < static_cast<int>(hist.size()); ++i) {
            if (hist[static_cast<std::size_t>(i)] != 0) {
                hmin = i;
                break;
            }
        }
        for (int i = static_cast<int>(hist.size()) - 1; i >= 0; --i) {
            if (hist[static_cast<std::size_t>(i)] != 0) {
                hmax = i;
                break;
            }
        }
        mutableParams.insert(QStringLiteral("stat_hmin"), hmin);
        mutableParams.insert(QStringLiteral("stat_hmax"), hmax);
    }

    // Check if GPU path is available for this algorithm
    if (GpuEffectPipeline::supportsAlgorithm(algorithmId)) {
        gpuPrevOut_ = outImage_;
        gpuApplyPending_ = true;
        emit pendingGpuApply(outImage_, algorithmId, mutableParams);
        return;  // async; commitGpuResult() will finish
    }

    // CPU path (unchanged from Phase 3)
    auto prevOut = outImage_;
    auto scratch = std::make_shared<ImageBuffer>();
    if (!ImageProcessorCore::apply(*outImage_, *scratch, algorithmId, mutableParams)) {
        emit errorOccurred(QStringLiteral("Failed to apply algorithm"));
        return;
    }

    pushCommand(std::make_unique<StaticApplyCommand>(this, prevOut, scratch));
    outImage_ = scratch;
    ++outImageVersion_;
    emit imageChanged();
    emit canSaveChanged();
    emit historyChanged();
}

void ProcessingController::undo()
{
    if (!canUndo())
        return;

    history_[static_cast<std::size_t>(historyIndex_)]->undo();
    --historyIndex_;
    emit historyChanged();
}

void ProcessingController::redo()
{
    if (!canRedo())
        return;

    ++historyIndex_;
    history_[static_cast<std::size_t>(historyIndex_)]->redo();
    emit historyChanged();
}

void ProcessingController::clearHistory()
{
    history_.clear();
    historyIndex_ = -1;
    historyBytes_ = 0;
    emit historyChanged();
}

void ProcessingController::pushCommand(std::unique_ptr<EditCommand> command)
{
    const std::size_t keepCount = static_cast<std::size_t>(historyIndex_ + 1);
    while (history_.size() > keepCount) {
        historyBytes_ -= history_.back()->memoryBytes();
        history_.pop_back();
    }

    historyBytes_ += command->memoryBytes();
    history_.push_back(std::move(command));
    historyIndex_ = static_cast<int>(history_.size()) - 1;

    while (history_.size() > 1
           && (history_.size() > MaxHistoryCount || historyBytes_ > MaxHistoryBytes)) {
        historyBytes_ -= history_.front()->memoryBytes();
        history_.pop_front();
        --historyIndex_;
    }
}

void ProcessingController::commitGpuResult(std::shared_ptr<ImageBuffer> result)
{
    if (!gpuApplyPending_) return;
    gpuApplyPending_ = false;
    pushCommand(std::make_unique<StaticApplyCommand>(this, gpuPrevOut_, result));
    gpuPrevOut_.reset();
    outImage_ = result;
    ++outImageVersion_;
    emit imageChanged();
    emit canSaveChanged();
    emit historyChanged();
}

void ProcessingController::cancelGpuApply()
{
    gpuApplyPending_ = false;
    gpuPrevOut_.reset();
    emit errorOccurred(QStringLiteral("GPU effect failed; result unchanged"));
}

bool ProcessingController::appendEffect(int algorithmId, const QVariantMap &params)
{
    if (static_cast<int>(effectStack_.size()) >= MaxEffectStack) {
        return false;
    }

    effectStack_.push_back({algorithmId, params});
    emit effectStackChanged();

    // Re-apply stack to current inImage_ so user sees result immediately
    if (inImage_) {
        auto result = applyEffectStack(inImage_);
        outImage_ = result;
        ++outImageVersion_;
        emit imageChanged();
    }

    return true;
}

void ProcessingController::removeEffect(int index)
{
    if (index < 0 || index >= static_cast<int>(effectStack_.size())) {
        return;
    }

    effectStack_.erase(effectStack_.begin() + index);
    emit effectStackChanged();

    if (inImage_) {
        outImage_ = applyEffectStack(inImage_);
        ++outImageVersion_;
        emit imageChanged();
    }
}

void ProcessingController::clearEffectStack()
{
    if (effectStack_.empty()) {
        return;
    }

    effectStack_.clear();
    emit effectStackChanged();

    if (inImage_) {
        outImage_ = std::make_shared<ImageBuffer>(*inImage_);
        ++outImageVersion_;
        emit imageChanged();
    }
}

std::shared_ptr<ImageBuffer> ProcessingController::applyEffectStack(std::shared_ptr<ImageBuffer> src)
{
    auto cur = src;
    for (const auto &e : effectStack_) {
        auto out = std::make_shared<ImageBuffer>();
        if (!ImageProcessorCore::apply(*cur, *out, e.algorithmId, e.params)) {
            break;
        }
        cur = out;
    }
    return cur;
}

void ProcessingController::onVideoFrame(std::shared_ptr<ImageBuffer> frame)
{
    inImage_ = frame;
    ++inImageVersion_;
    outImage_ = applyEffectStack(frame);
    ++outImageVersion_;
    emit imageChanged();
    if (!hasImage()) {
        emit hasImageChanged();
    }
}

void ProcessingController::onVideoPosition(double secs)
{
    videoPosition_ = secs;
    emit videoPositionChanged(secs);
}

void ProcessingController::onVideoPlaybackFinished()
{
    emit videoPlayingChanged();
}

void ProcessingController::playVideo()
{
    if (videoService_) {
        videoService_->play();
        emit videoPlayingChanged();
    }
}

void ProcessingController::pauseVideo()
{
    if (videoService_) {
        videoService_->pause();
        emit videoPlayingChanged();
    }
}

void ProcessingController::stepForwardVideo()
{
    if (videoService_) {
        videoService_->stepForward();
    }
}

void ProcessingController::stepBackwardVideo()
{
    if (videoService_) {
        videoService_->stepBackward();
    }
}

void ProcessingController::seekVideo(double secs)
{
    if (videoService_) {
        videoService_->seekToSecs(secs);
    }
}

void ProcessingController::setVideoLoop(bool loop)
{
    if (videoService_) {
        videoService_->setLoop(loop);
    }
}

void ProcessingController::setVideoSpeed(double speed)
{
    if (videoService_) {
        videoService_->setSpeed(speed);
    }
}
