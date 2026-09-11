#ifndef GASSCENEITEMS_H
#define GASSCENEITEMS_H

#include <QGraphicsObject>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QPen>
#include <QBrush>
#include <QPainterPath>
#include <QPixmap>
#include <QList>
#include <QGraphicsSceneMouseEvent>

class QGraphicsScene;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class Communication;
class ControlTab;

//==========================================================
// ValveItem - 可点击阀门（NV.png），带状态方块
//==========================================================
class ValveItem : public QGraphicsObject
{
    Q_OBJECT
public:
    ValveItem(const QString &name, const QPointF &pos, qreal rotation = 0,
              QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    QString name() const { return m_name; }
    bool state() const { return m_state; }
    void setState(bool on);
    void setLabelOffset(const QPointF &offset);

signals:
    void clicked(ValveItem *item);
    void stateChanged(const QString &name, bool state);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_name;
    bool m_state;
    QGraphicsPixmapItem *m_pixmapItem;
    QGraphicsRectItem *m_stateRect;
    QGraphicsTextItem *m_label;
};

//==========================================================
// SixWayValveItem - 六通阀
//==========================================================
class SixWayValveItem : public QGraphicsObject
{
    Q_OBJECT
public:
    SixWayValveItem(const QString &name, const QPointF &center,
                    QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    QString name() const { return m_name; }
    bool state() const { return m_state; }
    void setState(bool on);

signals:
    void clicked(SixWayValveItem *item);
    void stateChanged(const QString &name, bool state);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void updateInternalConnections();

    QString m_name;
    bool m_state;
    QGraphicsEllipseItem *m_circle;
    QList<QGraphicsEllipseItem*> m_ports;
    QGraphicsPathItem *m_internalPath;
    QGraphicsTextItem *m_label;
};

//==========================================================
// DoubleClickableRect - 双击区域
//==========================================================
class DoubleClickableRect : public QGraphicsObject
{
    Q_OBJECT
public:
    DoubleClickableRect(const QRectF &rect, const QString &title, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

signals:
    void doubleClicked(const QString &title);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QRectF m_rect;
    QString m_title;
};

//==========================================================
// 辅助函数
//==========================================================
void addTCD(QGraphicsScene *scene, const QPointF &pos);
void buildGasScene(QGraphicsScene *scene, Communication *comm, ControlTab *ctrl);

#endif // GASSCENEITEMS_H