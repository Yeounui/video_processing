#pragma once

#include <QQuickItem>
#include <QPointer>
#include <QPointF>
#include <QtQml/qqmlregistration.h>
#include <limits>

class ProcessingController;

class ProcessingViewportItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(ProcessingViewportItem)
    Q_PROPERTY(ProcessingController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool showOriginal READ showOriginal WRITE setShowOriginal NOTIFY showOriginalChanged)
    Q_PROPERTY(bool splitCompare READ splitCompare WRITE setSplitCompare NOTIFY splitCompareChanged)
    Q_PROPERTY(qreal splitPosition READ splitPosition WRITE setSplitPosition NOTIFY splitPositionChanged)
    Q_PROPERTY(qreal zoomScale READ zoomScale WRITE setZoomScale NOTIFY zoomScaleChanged)
    Q_PROPERTY(qreal panOffsetX READ panOffsetX WRITE setPanOffsetX NOTIFY panOffsetChanged)
    Q_PROPERTY(qreal panOffsetY READ panOffsetY WRITE setPanOffsetY NOTIFY panOffsetChanged)

public:
    explicit ProcessingViewportItem(QQuickItem *parent = nullptr);

    ProcessingController *controller() const;
    void setController(ProcessingController *ctrl);

    bool showOriginal() const;
    void setShowOriginal(bool v);

    bool splitCompare() const;
    void setSplitCompare(bool v);

    qreal splitPosition() const;
    void setSplitPosition(qreal v);

    qreal zoomScale() const;
    void setZoomScale(qreal v);

    qreal panOffsetX() const;
    void setPanOffsetX(qreal v);

    qreal panOffsetY() const;
    void setPanOffsetY(qreal v);

    Q_INVOKABLE void resetView();

signals:
    void controllerChanged();
    void showOriginalChanged();
    void splitCompareChanged();
    void splitPositionChanged();
    void zoomScaleChanged();
    void panOffsetChanged();
    void renderErrorOccurred(const QString &message);

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onImageChanged();

private:
    QPointer<ProcessingController> controller_;
    quint64 uploadedOutVersion_ = std::numeric_limits<quint64>::max();
    quint64 uploadedInVersion_  = std::numeric_limits<quint64>::max();

    bool  showOriginal_   = false;
    bool  splitCompare_   = false;
    qreal splitPosition_  = 0.5;
    qreal zoomScale_      = 1.0;
    qreal panOffsetX_     = 0.0;  // in source-image pixels
    qreal panOffsetY_     = 0.0;
    int   lastImageWidth_ = 0;
    int   lastImageHeight_ = 0;

    // Drag-pan state
    bool   dragging_       = false;
    QPointF dragStart_;
    qreal  dragStartPanX_ = 0.0;
    qreal  dragStartPanY_ = 0.0;
};
