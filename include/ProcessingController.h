#pragma once

#include "EditCommand.h"
#include "ImageBuffer.h"
#include <deque>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <memory>

class AlgorithmModel;

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

    // Public (non-QML) accessors
    std::shared_ptr<ImageBuffer> inImage() const;
    std::shared_ptr<ImageBuffer> outImage() const;
    quint64 inImageVersion() const;
    quint64 outImageVersion() const;
    void setOutImageDirect(std::shared_ptr<ImageBuffer> img);

    // Public slots (Q_INVOKABLE)
    Q_INVOKABLE void openImage(const QUrl &url);
    Q_INVOKABLE void saveImage(const QUrl &url);
    Q_INVOKABLE void reset();
    Q_INVOKABLE void applyAlgorithm(int algorithmId, const QVariantMap &params);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void hasImageChanged();
    void canSaveChanged();
    void historyChanged();
    void sourceChanged();
    void imageChanged();
    void errorOccurred(const QString &message);

private:
    void clearHistory();
    void pushCommand(std::unique_ptr<EditCommand> command);

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
};
