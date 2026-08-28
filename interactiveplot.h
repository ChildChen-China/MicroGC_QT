#ifndef INTERACTIVEPLOT_H
#define INTERACTIVEPLOT_H

#include "qcustomplot.h"
#include <QPointF>
#include <QString>
#include <QPoint>
#include <QEvent>

class InteractivePlot : public QCustomPlot
{
    Q_OBJECT

public:
    enum ZoomMode {
        ZoomX,
        ZoomY,
        ZoomXY,
        ZoomRect
    };

    explicit InteractivePlot(QWidget *parent = nullptr);
    ~InteractivePlot() override = default;

    void setZoomMode(ZoomMode mode);
    ZoomMode zoomMode() const { return m_zoomMode; }

    void setCrosshairEnabled(bool enabled);
    bool isCrosshairEnabled() const { return m_crosshairEnabled; }

    void setCrosshairName(const QString &name);
    QString crosshairName() const { return m_crosshairName; }

    void resetView(double xLower = 0.0, double xUpper = 5.0,
                   double yLower = 0.0, double yUpper = 5.0);

signals:
    void clicked(const QPointF &scenePos);       // 左键点击（非拖拽）
    void rightClicked(const QPointF &scenePos);  // 右键点击

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;      // 新增：鼠标离开时隐藏十字标

private:
    void updateCrosshair(const QPoint &pos);

    ZoomMode m_zoomMode;
    bool m_crosshairEnabled;
    QString m_crosshairName;

    bool m_dragging;
    bool m_selecting;
    bool m_potentialClick;
    QPoint m_pressPos;
    QPoint m_origin;
    QPoint m_lastPos;
    QRubberBand *m_rubberBand;

    QCPItemLine *m_vLine;
    QCPItemLine *m_hLine;
    QCPItemText *m_crosshairLabel;
};

#endif // INTERACTIVEPLOT_H