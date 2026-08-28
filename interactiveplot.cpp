#include "interactiveplot.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QRubberBand>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QApplication>

InteractivePlot::InteractivePlot(QWidget *parent)
    : QCustomPlot(parent)
    , m_zoomMode(ZoomXY)
    , m_crosshairEnabled(false)
    , m_crosshairName("")
    , m_dragging(false)
    , m_selecting(false)
    , m_potentialClick(false)
    , m_rubberBand(nullptr)
    , m_vLine(nullptr)
    , m_hLine(nullptr)
    , m_crosshairLabel(nullptr)
{
    setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // 备用

    // ========== 性能优化 ==========
    // 快速线条绘制（尤其适合线宽为1的曲线）
    setPlottingHints(QCP::phFastPolylines);
    // 拖动过程中禁用抗锯齿，提高响应速度
    setNoAntialiasingOnDrag(true);
    // 如果您的 QCustomPlot 编译时启用了 OpenGL，可以取消下一行注释以利用 GPU 加速
    // setOpenGl(true);
    // ================================

    // 创建十字标元素，初始隐藏
    m_vLine = new QCPItemLine(this);
    m_vLine->setPen(QPen(Qt::DashLine));
    m_vLine->setVisible(false);
    m_vLine->setLayer("overlay");

    m_hLine = new QCPItemLine(this);
    m_hLine->setPen(QPen(Qt::DashLine));
    m_hLine->setVisible(false);
    m_hLine->setLayer("overlay");

    m_crosshairLabel = new QCPItemText(this);
    m_crosshairLabel->setVisible(false);
    m_crosshairLabel->setLayer("overlay");
    m_crosshairLabel->setPadding(QMargins(4,4,4,4));
    m_crosshairLabel->setBrush(QBrush(QColor(255,255,255,200)));
    m_crosshairLabel->setPen(QPen(Qt::black));
    m_crosshairLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_crosshairLabel->setClipToAxisRect(false);
}

void InteractivePlot::setZoomMode(ZoomMode mode)
{
    m_zoomMode = mode;
}

void InteractivePlot::setCrosshairEnabled(bool enabled)
{
    m_crosshairEnabled = enabled;
    if (m_vLine) m_vLine->setVisible(enabled);
    if (m_hLine) m_hLine->setVisible(enabled);
    if (m_crosshairLabel) m_crosshairLabel->setVisible(enabled);
    setMouseTracking(enabled);   // 需要鼠标追踪才能实时更新十字标
    replot();
}

void InteractivePlot::setCrosshairName(const QString &name)
{
    m_crosshairName = name;
    if (m_crosshairLabel) {
        m_crosshairLabel->setText(name + ": x=--, y=--");
    }
}

void InteractivePlot::resetView(double xLower, double xUpper,
                                double yLower, double yUpper)
{
    xAxis->setRange(xLower, xUpper);
    yAxis->setRange(yLower, yUpper);
    replot();
}

void InteractivePlot::mousePressEvent(QMouseEvent *event)
{
    // 右键直接发射 rightClicked 信号，不参与拖拽/缩放
    if (event->button() == Qt::RightButton) {
        double x = xAxis->pixelToCoord(event->pos().x());
        double y = yAxis->pixelToCoord(event->pos().y());
        emit rightClicked(QPointF(x, y));
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_zoomMode == ZoomRect) {
            m_selecting = true;
            m_origin = event->pos();
            if (!m_rubberBand) {
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
            }
            m_rubberBand->setGeometry(QRect(m_origin, m_origin));
            m_rubberBand->show();
            event->accept();
            return;
        } else {
            // 先记录为潜在点击，不立即开始拖拽
            m_potentialClick = true;
            m_pressPos = event->pos();
            event->accept();
            return;
        }
    }
    QCustomPlot::mousePressEvent(event);
}

void InteractivePlot::mouseMoveEvent(QMouseEvent *event)
{
    if (m_crosshairEnabled) {
        // 确保十字标可见
        m_vLine->setVisible(true);
        m_hLine->setVisible(true);
        m_crosshairLabel->setVisible(true);
        updateCrosshair(event->pos());
    }

    if (m_selecting && m_rubberBand) {
        m_rubberBand->setGeometry(QRect(m_origin, event->pos()).normalized());
        event->accept();
        return;
    }

    // 如果可能是点击，检查移动距离
    if (m_potentialClick) {
        if ((event->pos() - m_pressPos).manhattanLength() > QApplication::startDragDistance()) {
            m_potentialClick = false;
            m_dragging = true;
            m_lastPos = m_pressPos;
            setCursor(Qt::ClosedHandCursor);
        } else {
            event->accept();
            return;
        }
    }

    if (m_dragging) {
        QPoint delta = event->pos() - m_lastPos;
        if (delta.manhattanLength() < 2) {
            event->accept();
            return;
        }

        double xMove = xAxis->pixelToCoord(event->pos().x()) -
                       xAxis->pixelToCoord(m_lastPos.x());
        double yMove = yAxis->pixelToCoord(m_lastPos.y()) -
                       yAxis->pixelToCoord(event->pos().y());

        xAxis->setRange(xAxis->range().lower - xMove,
                        xAxis->range().upper - xMove);
        yAxis->setRange(yAxis->range().lower + yMove,
                        yAxis->range().upper + yMove);

        m_lastPos = event->pos();
        replot();
        event->accept();
        return;
    }

    if (m_crosshairEnabled) {
        replot();
    }
    QCustomPlot::mouseMoveEvent(event);
}

void InteractivePlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selecting = false;
        if (m_rubberBand) {
            m_rubberBand->hide();
            QRect rect = QRect(m_origin, event->pos()).normalized();
            if (rect.width() > 5 && rect.height() > 5) {
                double xLower = xAxis->pixelToCoord(rect.left());
                double xUpper = xAxis->pixelToCoord(rect.right());
                double yLower = yAxis->pixelToCoord(rect.bottom());
                double yUpper = yAxis->pixelToCoord(rect.top());
                xAxis->setRange(xLower, xUpper);
                yAxis->setRange(yLower, yUpper);
                replot();
            }
        }
        event->accept();
        return;
    }

    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (m_potentialClick && event->button() == Qt::LeftButton) {
        m_potentialClick = false;
        QPointF scenePos(xAxis->pixelToCoord(event->pos().x()),
                         yAxis->pixelToCoord(event->pos().y()));
        emit clicked(scenePos);
        event->accept();
        return;
    }

    QCustomPlot::mouseReleaseEvent(event);
}

void InteractivePlot::wheelEvent(QWheelEvent *event)
{
    double factor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;
    double centerX = xAxis->pixelToCoord(event->position().x());
    double centerY = yAxis->pixelToCoord(event->position().y());

    if (m_zoomMode == ZoomX || m_zoomMode == ZoomXY || m_zoomMode == ZoomRect) {
        double xRange = xAxis->range().size() * factor;
        xAxis->setRange(centerX - (centerX - xAxis->range().lower) * factor,
                        centerX + (xAxis->range().upper - centerX) * factor);
    }
    if (m_zoomMode == ZoomY || m_zoomMode == ZoomXY || m_zoomMode == ZoomRect) {
        double yRange = yAxis->range().size() * factor;
        yAxis->setRange(centerY - (centerY - yAxis->range().lower) * factor,
                        centerY + (yAxis->range().upper - centerY) * factor);
    }

    replot();
    event->accept();
}

void InteractivePlot::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (m_crosshairEnabled) {
        m_vLine->setVisible(false);
        m_hLine->setVisible(false);
        m_crosshairLabel->setVisible(false);
        replot();
    }
}

void InteractivePlot::updateCrosshair(const QPoint &pos)
{
    if (!m_vLine || !m_hLine || !m_crosshairLabel)
        return;

    double x = xAxis->pixelToCoord(pos.x());
    double y = yAxis->pixelToCoord(pos.y());

    m_vLine->start->setCoords(x, yAxis->range().lower);
    m_vLine->end->setCoords(x, yAxis->range().upper);
    m_hLine->start->setCoords(xAxis->range().lower, y);
    m_hLine->end->setCoords(xAxis->range().upper, y);

    QString text = QString("%1: x=%2, y=%3")
                       .arg(m_crosshairName.isEmpty() ? "Crosshair" : m_crosshairName)
                       .arg(x, 0, 'f', 3)
                       .arg(y, 0, 'f', 3);
    m_crosshairLabel->setText(text);
    m_crosshairLabel->position->setCoords(x, y);
    m_crosshairLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
}