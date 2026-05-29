#pragma once

#include "ImageBuffer.h"
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <memory>

class ProcessingController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY hasImageChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY canSaveChanged)
    Q_PROPERTY(QString sourceFileName READ sourceFileName NOTIFY sourceChanged)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY sourceChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY sourceChanged)

public:
    explicit ProcessingController(QObject *parent = nullptr);

    // Q_PROPERTY read accessors
    bool hasImage() const;
    bool canSave() const;
    QString sourceFileName() const;
    int imageWidth() const;
    int imageHeight() const;

    // Public (non-QML) accessors
    std::shared_ptr<ImageBuffer> outImage() const;
    quint64 outImageVersion() const;

    // Public slots (Q_INVOKABLE)
    Q_INVOKABLE void openImage(const QUrl &url);
    Q_INVOKABLE void saveImage(const QUrl &url);
    Q_INVOKABLE void reset();

signals:
    void hasImageChanged();
    void canSaveChanged();
    void sourceChanged();
    void imageChanged();
    void errorOccurred(const QString &message);

private:
    std::shared_ptr<ImageBuffer> inImage_;
    std::shared_ptr<ImageBuffer> outImage_;
    quint64 outImageVersion_ = 0;
    SourceType sourceType_ = SourceType::SOURCE_NONE;
    QString sourceFileName_;
};
