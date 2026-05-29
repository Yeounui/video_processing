#pragma once

#include <QQuickItem>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <limits>

class ProcessingController;

class ProcessingViewportItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(ProcessingViewportItem)
    Q_PROPERTY(ProcessingController* controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit ProcessingViewportItem(QQuickItem *parent = nullptr);

    ProcessingController *controller() const;
    void setController(ProcessingController *ctrl);

signals:
    void controllerChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private slots:
    void onImageChanged();

private:
    QPointer<ProcessingController> controller_;
    quint64 uploadedVersion_ = std::numeric_limits<quint64>::max();
};
