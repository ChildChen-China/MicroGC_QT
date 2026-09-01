#include "gassceneitems.h"
#include "controltab.h"
#include "communication.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsItemGroup>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <QDebug>

//-------------------------------------------------------------
// ValveItem 实现
//-------------------------------------------------------------
ValveItem::ValveItem(const QString &name, const QPointF &pos, qreal rotation, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_name(name)
    , m_state(false)
{
    setPos(pos);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setAcceptHoverEvents(true);

    QPixmap pixmap(":/img/NV.png");
    if (!pixmap.isNull()) {
        m_pixmapItem = new QGraphicsPixmapItem(pixmap, this);
        m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
        qreal scale = 40.0 / pixmap.width() + 0.3;
        m_pixmapItem->setScale(scale);
        m_pixmapItem->setOffset((-pixmap.width()*scale/2)-33, (-pixmap.height()*scale/2)-13);
        m_pixmapItem->setRotation(rotation);
    } else {
        m_pixmapItem = nullptr;
    }

    qreal blockWidth = 44;
    qreal blockHeight = 20;
    qreal blockY = -37;
    m_stateRect = new QGraphicsRectItem((-blockWidth/2)-5, blockY, blockWidth, blockHeight, this);
    m_stateRect->setBrush(QBrush(QColor("#a0a0a0")));
    m_stateRect->setPen(QPen(Qt::black, 1));
    m_stateRect->setTransformOriginPoint(0, 0);
    m_stateRect->setRotation(rotation);

    m_label = new QGraphicsTextItem(m_name, this);
    m_label->setDefaultTextColor(QColor("#0055aa"));
    m_label->setFont(QFont("Arial", 12, QFont::Bold));
    m_label->setPos(0, 20);
}

QRectF ValveItem::boundingRect() const
{
    return QRectF(-30, -40, 60, 100);
}

void ValveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                      QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

void ValveItem::setState(bool on)
{
    if (m_state == on)
        return;
    m_state = on;

    if (m_stateRect) {
        QColor color = on ? QColor("#e74c3c") : QColor("#a0a0a0");
        m_stateRect->setBrush(QBrush(color));
    }
    emit stateChanged(m_name, m_state);
}

void ValveItem::setLabelOffset(const QPointF &offset)
{
    if (m_label) m_label->setPos(offset);
}

void ValveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(this);
        event->accept();
    } else {
        QGraphicsObject::mousePressEvent(event);
    }
}

//-------------------------------------------------------------
// SixWayValveItem 实现
//-------------------------------------------------------------
SixWayValveItem::SixWayValveItem(const QString &name, const QPointF &center, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_name(name)
    , m_state(false)
{
    setPos(center);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setAcceptHoverEvents(true);

    m_circle = new QGraphicsEllipseItem(-60, -60, 120, 120, this);
    m_circle->setBrush(QBrush(QColor("#f0f0f0")));
    m_circle->setPen(QPen(QColor("#333333"), 2.5));

    auto *innerCircle = new QGraphicsEllipseItem(-45, -45, 90, 90, this);
    innerCircle->setBrush(QBrush(Qt::white));
    innerCircle->setPen(QPen(QColor("#cccccc"), 1));

    for (int i = 0; i < 6; ++i) {
        double angle = -90 + i * 60;
        double x = 55 * qCos(qDegreesToRadians(angle));
        double y = 55 * qSin(qDegreesToRadians(angle));
        auto *port = new QGraphicsEllipseItem(x - 6, y - 6, 12, 12, this);
        port->setBrush(QBrush(QColor("#888888")));
        port->setPen(QPen(Qt::black, 2));
        m_ports.append(port);
    }

    // 修正端口编号位置：半径改为28，字体略小，确保5和6显示正常
    for (int i = 0; i < 6; ++i) {
        double angle = -90 + i * 60;
        double xInner = 28 * qCos(qDegreesToRadians(angle));
        double yInner = 28 * qSin(qDegreesToRadians(angle));
        auto *portLabel = new QGraphicsTextItem(QString::number(i + 1), this);
        portLabel->setFont(QFont("Arial", 15, QFont::Bold));
        portLabel->setDefaultTextColor(Qt::black);
        portLabel->setPos(xInner - 5, yInner - 8);
    }

    m_internalPath = new QGraphicsPathItem(this);
    m_internalPath->setZValue(10);
    updateInternalConnections();

    m_label = new QGraphicsTextItem(m_name, this);
    m_label->setDefaultTextColor(Qt::black);
    m_label->setFont(QFont("Arial", 12, QFont::Bold));
    m_label->setPos(-30, 75);
}

QRectF SixWayValveItem::boundingRect() const
{
    return QRectF(-70, -70, 140, 160);
}

void SixWayValveItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

void SixWayValveItem::setState(bool on)
{
    if (m_state == on)
        return;
    m_state = on;
    updateInternalConnections();
    emit stateChanged(m_name, m_state);
}

void SixWayValveItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(this);
        event->accept();
    } else {
        QGraphicsObject::mousePressEvent(event);
    }
}

void SixWayValveItem::updateInternalConnections()
{
    QPainterPath path;
    QPen pen;
    if (!m_state) {
        pen = QPen(QColor("#0055cc"), 3);
        path.moveTo(0, -55);
        path.lineTo(47.6, -27.5);
        path.moveTo(47.6, 27.5);
        path.lineTo(0, 55);
        path.moveTo(-47.6, 27.5);
        path.lineTo(-47.6, -27.5);
    } else {
        pen = QPen(QColor("#e67e22"), 3);
        path.moveTo(47.6, -27.5);
        path.lineTo(47.6, 27.5);
        path.moveTo(0, 55);
        path.lineTo(-47.6, 27.5);
        path.moveTo(-47.6, -27.5);
        path.lineTo(0, -55);
    }
    m_internalPath->setPath(path);
    m_internalPath->setPen(pen);
}

//-------------------------------------------------------------
// DoubleClickableRect 实现
//-------------------------------------------------------------
DoubleClickableRect::DoubleClickableRect(const QRectF &rect, const QString &title, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_rect(rect), m_title(title)
{
    setFlag(QGraphicsItem::ItemHasNoContents, true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF DoubleClickableRect::boundingRect() const
{
    return m_rect;
}

void DoubleClickableRect::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                QWidget *widget)
{
    Q_UNUSED(painter);
    Q_UNUSED(option);
    Q_UNUSED(widget);
}

void DoubleClickableRect::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit doubleClicked(m_title);
    QGraphicsObject::mouseDoubleClickEvent(event);
}

//-------------------------------------------------------------
// 辅助函数：加箭头（醒目大箭头，支持旋转）
//-------------------------------------------------------------
void addArrow(QGraphicsScene *scene, const QPointF &pos, bool right, qreal angle = 0.0)
{
    QPolygonF arrow;
    if (right) {
        arrow << QPointF(pos.x(), pos.y() - 10)
        << QPointF(pos.x() + 16, pos.y())
        << QPointF(pos.x(), pos.y() + 10);
    } else {
        arrow << QPointF(pos.x() + 16, pos.y() - 10)
        << QPointF(pos.x(), pos.y())
        << QPointF(pos.x() + 16, pos.y() + 10);
    }
    auto *item = new QGraphicsPolygonItem(arrow);
    item->setBrush(Qt::black);
    item->setPen(QPen(Qt::black, 2));
    if (right) {
        item->setTransformOriginPoint(pos.x() + 16, pos.y());
    } else {
        item->setTransformOriginPoint(pos.x(), pos.y());
    }
    item->setRotation(angle);
    scene->addItem(item);
}

//-------------------------------------------------------------
// 辅助函数：微调阀
//-------------------------------------------------------------
void addFineAdjustValve(QGraphicsScene *scene, const QPointF &pos)
{
    auto *body = new QGraphicsRectItem(pos.x() - 13, pos.y() - 40, 26, 81);
    body->setBrush(Qt::white);
    body->setPen(QPen(Qt::black, 2));
    scene->addItem(body);

    auto *wheel = new QGraphicsLineItem(pos.x(), pos.y() - 40, pos.x(), pos.y() - 58);
    wheel->setPen(QPen(Qt::black, 2));
    scene->addItem(wheel);
    auto *wheelBar = new QGraphicsLineItem(pos.x() - 15, pos.y() - 58, pos.x() + 15, pos.y() - 58);
    wheelBar->setPen(QPen(Qt::black, 2));
    scene->addItem(wheelBar);
    auto *wheelStem = new QGraphicsLineItem(pos.x(), pos.y() - 71, pos.x(), pos.y() - 58);
    wheelStem->setPen(QPen(Qt::black, 2));
    scene->addItem(wheelStem);
}

//-------------------------------------------------------------
// 辅助函数：长方形过滤器
//-------------------------------------------------------------
void addFilterRect(QGraphicsScene *scene, const QPointF &center)
{
    auto *filter = new QGraphicsRectItem(center.x() - 25, center.y() - 14, 50, 28);
    filter->setBrush(Qt::white);
    filter->setPen(QPen(Qt::black, 2));
    scene->addItem(filter);

    scene->addLine(center.x() - 15, center.y(), center.x() + 15, center.y(), QPen(Qt::black, 1));
    scene->addLine(center.x(), center.y() - 8, center.x(), center.y() + 8, QPen(Qt::black, 1));
}

//-------------------------------------------------------------
// 专业TCD绘制函数
//-------------------------------------------------------------
void addTCD(QGraphicsScene *scene, const QPointF &pos)
{
    qreal x = pos.x();
    qreal y = pos.y();

    auto *frame = new QGraphicsRectItem(x, y, 417, 210);
    frame->setBrush(QBrush(QColor("#e8e8e8")));
    frame->setPen(QPen(QColor("#333333"), 3));
    scene->addItem(frame);

    for (int i = 0; i < 8; ++i) {
        auto *fin = new QGraphicsRectItem(x + 30 + i*45, y + 5, 30, 18);
        fin->setBrush(QBrush(QColor("#b0b0b0")));
        fin->setPen(QPen(QColor("#555555"), 1));
        scene->addItem(fin);
    }

    auto *dash = new QGraphicsRectItem(x+41, y+35, 335, 155);
    dash->setBrush(Qt::NoBrush);
    dash->setPen(QPen(Qt::black, 1.6, Qt::DashLine));
    scene->addItem(dash);

    QList<QPointF> cells = { {x+106, y+110}, {x+221, y+110}, {x+336, y+110} };
    for (const QPointF &c : cells) {
        auto *cell = new QGraphicsEllipseItem(c.x()-24, c.y()-24, 48, 48);
        cell->setBrush(QBrush(QColor("#202020")));
        cell->setPen(QPen(QColor("#888888"), 1.5));
        scene->addItem(cell);
        auto *highlight = new QGraphicsEllipseItem(c.x()-16, c.y()-16, 12, 12);
        highlight->setBrush(QBrush(QColor("#404040")));
        highlight->setPen(Qt::NoPen);
        scene->addItem(highlight);
        scene->addLine(c.x(), c.y()+24, c.x(), c.y()+52, QPen(Qt::black, 1.6));
    }

    // 此处不添加固定的“5TCD-001”标签，留待动态更新
}

//-------------------------------------------------------------
// buildGasScene 函数实现
//-------------------------------------------------------------
void buildGasScene(QGraphicsScene *scene, Communication *comm, ControlTab *ctrl)
{
    QPen blackPen(Qt::black, 3.5);
    QPen thinPen(Qt::black, 1.6);

    // ========== 一、驱动气路 ==========
    scene->addLine(215, 252, 649, 252, blackPen);
    addArrow(scene, QPointF(418, 252), true);

    ValveItem *nv1 = new ValveItem("", QPointF(692, 253));
    scene->addItem(nv1);

    scene->addLine(729, 252, 1075, 252, blackPen);
    addArrow(scene, QPointF(868, 252), true);
    scene->addLine(1073, 252, 1073, 415, blackPen);
    scene->addLine(1073, 415, 1234, 415, blackPen);
    addArrow(scene, QPointF(1108, 415), true);
    scene->addLine(1232, 421, 1232, 946, blackPen);
    addArrow(scene, QPointF(1222, 700), false, 30);

    // ========== 二、载气路 ==========
    scene->addLine(316, 515, 593, 515, blackPen);
    addArrow(scene, QPointF(418, 515), true);
    scene->addLine(591, 455, 591, 598, blackPen);
    scene->addLine(591, 455, 1008, 455, blackPen);
    addArrow(scene, QPointF(628, 455), true);
    scene->addLine(591, 598, 722, 598, blackPen);
    addArrow(scene, QPointF(628, 598), true);

    QPixmap flowPixmap(":/img/Flow.png");
    if (!flowPixmap.isNull()) {
        auto *flow1Item = new QGraphicsPixmapItem(flowPixmap);
        flow1Item->setScale(1.5);
        flow1Item->setPos(739, 382);
        scene->addItem(flow1Item);
    }

    addFineAdjustValve(scene, QPointF(948, 415));
    scene->addLine(1006, 452, 1797, 452, blackPen);
    addArrow(scene, QPointF(1138, 452), true);

    if (!flowPixmap.isNull()) {
        auto *flow2Item = new QGraphicsPixmapItem(flowPixmap);
        flow2Item->setScale(1.5);
        flow2Item->setPos(739, 520);
        scene->addItem(flow2Item);
    }

    scene->addLine(838, 598, 1308, 598, blackPen);
    addArrow(scene, QPointF(966, 598), true);
    scene->addLine(1306, 598, 1306, 1001, blackPen);
    addArrow(scene, QPointF(1298, 800), false, 30);

    // ========== 三、柱温箱 ==========
    auto *ovenFrame = new QGraphicsRectItem(1209, 49, 302, 278);
    ovenFrame->setBrush(QBrush(QColor("#e8e8e8")));
    ovenFrame->setPen(QPen(QColor("#333333"), 3));
    scene->addItem(ovenFrame);

    for (int i = 0; i < 6; ++i) {
        auto *fin = new QGraphicsRectItem(1220 + i*42, 54, 30, 15);
        fin->setBrush(QBrush(QColor("#b0b0b0")));
        fin->setPen(QPen(QColor("#555555"), 1));
        scene->addItem(fin);
    }

    auto *dash = new QGraphicsRectItem(1230, 80, 260, 230);
    dash->setBrush(Qt::NoBrush);
    dash->setPen(QPen(Qt::black, 1.6, Qt::DashLine));
    scene->addItem(dash);

    QList<QPointF> ports = { {1290, 150}, {1430, 150} };
    for (const QPointF &p : ports) {
        auto *circle = new QGraphicsEllipseItem(p.x()-15, p.y()-15, 30, 30);
        circle->setBrush(QBrush(QColor("#202020")));
        circle->setPen(QPen(QColor("#888888"), 1.5));
        scene->addItem(circle);
        auto *highlight = new QGraphicsEllipseItem(p.x()-8, p.y()-8, 8, 8);
        highlight->setBrush(QBrush(QColor("#404040")));
        highlight->setPen(Qt::NoPen);
        scene->addItem(highlight);
        scene->addLine(p.x(), p.y()+15, p.x(), p.y()+40, QPen(Qt::black, 1.6));
    }

    scene->addLine(1290, 190, 1430, 190, QPen(Qt::black, 2.5));
    scene->addLine(1290, 195, 1430, 195, QPen(Qt::black, 2.5));

    scene->addLine(1360, 310, 1360, 327, QPen(Qt::black, 2));

    // 动态温度标签：替代原来的“4-ZWX-001”
    auto *ovenTempLabel = new QGraphicsTextItem("-- ℃");
    ovenTempLabel->setObjectName("ovenTempLabel");
    ovenTempLabel->setFont(QFont("Arial", 22, QFont::Bold));
    ovenTempLabel->setDefaultTextColor(Qt::black);
    ovenTempLabel->setPos(1330, 230);
    scene->addItem(ovenTempLabel);

    scene->addLine(1338, 330, 1338, 997, blackPen);
    scene->addLine(1378, 330, 1378, 850, blackPen);
    scene->addLine(1378, 850, 1615, 850, blackPen);
    scene->addLine(1615, 850, 1615, 1090, blackPen);
    scene->addLine(1794, 456, 1794, 1090, blackPen);

    // ========== 四、样品气路 ==========
    scene->addLine(280, 959, 1258, 959, blackPen);
    addArrow(scene, QPointF(688, 959), true);
    addFilterRect(scene, QPointF(442, 959));
    auto *filterLabel = new QGraphicsTextItem("过滤器");
    filterLabel->setFont(QFont("Arial", 18, QFont::Bold));
    filterLabel->setDefaultTextColor(Qt::black);
    filterLabel->setPos(411, 976);
    scene->addItem(filterLabel);

    scene->addLine(1245, 955, 1245, 1103, blackPen);
    scene->addLine(1248, 1104, 324, 1104, blackPen);
    addArrow(scene, QPointF(1138, 1104), false);
    scene->addLine(1128, 1104, 1128, 1126, thinPen);

    // 压力传感器图片
    QPixmap volumePixmap(":/img/volume.png");
    if (!volumePixmap.isNull()) {
        auto *psItem = new QGraphicsPixmapItem(volumePixmap);
        psItem->setScale(1.2);
        psItem->setRotation(90);
        psItem->setPos(800, 980);
        scene->addItem(psItem);
    }

    // 压力传感器数值标签
    auto *pressureValueLabel = new QGraphicsTextItem("-- kPa");
    pressureValueLabel->setObjectName("pressureValueLabel");
    pressureValueLabel->setFont(QFont("Arial", 18, QFont::Bold));
    pressureValueLabel->setDefaultTextColor(Qt::black);
    pressureValueLabel->setPos(730, 1120);
    scene->addItem(pressureValueLabel);

    // 测温元件小正方形
    auto *tempRect = new QGraphicsRectItem(865, 1055, 40, 40);
    tempRect->setBrush(QBrush(QColor("#f0f0f0")));
    tempRect->setPen(QPen(Qt::black, 2));
    scene->addItem(tempRect);
    auto *tempLabel = new QGraphicsTextItem("测温元件");
    tempLabel->setFont(QFont("Arial", 17, QFont::Bold));
    tempLabel->setDefaultTextColor(Qt::black);
    tempLabel->setPos(850, 1120);
    scene->addItem(tempLabel);

    // ========== 五、六通阀 ==========
    SixWayValveItem *valve1 = new SixWayValveItem("六通阀", QPointF(1234, 1004));
    scene->addItem(valve1);

    scene->addLine(1275, 902, 1374, 902, blackPen);
    scene->addLine(1275, 1053, 1374, 1053, blackPen);
    scene->addLine(1374, 902, 1374, 1053, blackPen);
    scene->addLine(1275, 902, 1275, 1005, blackPen);
    scene->addLine(1275, 1053, 1275, 1005, blackPen);
    QPainterPath loopPath;
    loopPath.moveTo(1279, 1004);
    loopPath.lineTo(1300, 1004);
    loopPath.lineTo(1300, 1081);
    loopPath.cubicTo(1310, 1060, 1330, 1100, 1350, 1080);
    loopPath.cubicTo(1370, 1062, 1385, 1098, 1390, 1081);
    auto *loopItem = new QGraphicsPathItem(loopPath);
    loopItem->setPen(QPen(Qt::black, 5.5));
    scene->addItem(loopItem);
    // 删除原来的3-FX-001标签

    // ========== 六、检测器 ==========
    addTCD(scene, QPointF(1479, 1090));

    // TCD温度动态标签（原来5TCD-001位置）
    auto *tcdTempLabel = new QGraphicsTextItem("-- ℃");
    tcdTempLabel->setObjectName("tcdTempLabel");
    tcdTempLabel->setFont(QFont("Arial", 20, QFont::Bold));
    tcdTempLabel->setDefaultTextColor(Qt::black);
    tcdTempLabel->setPos(1600, 1240);
    scene->addItem(tcdTempLabel);

    scene->addLine(1621, 1300, 1621, 1321, blackPen);
    scene->addLine(1621, 1321, 346, 1321, blackPen);
    addArrow(scene, QPointF(1488, 1321), false);
    scene->addLine(1805, 1300, 1805, 1367, blackPen);
    scene->addLine(1805, 1367, 346, 1367, blackPen);
    addArrow(scene, QPointF(1588, 1367), false);

    // 流量标签
    auto *flow1ValueLabel = new QGraphicsTextItem("-- mL/min");
    flow1ValueLabel->setObjectName("flow1ValueLabel");
    flow1ValueLabel->setFont(QFont("Arial", 15, QFont::Bold));
    flow1ValueLabel->setDefaultTextColor(Qt::black);
    flow1ValueLabel->setPos(750, 480);
    scene->addItem(flow1ValueLabel);

    auto *flow2ValueLabel = new QGraphicsTextItem("-- mL/min");
    flow2ValueLabel->setObjectName("flow2ValueLabel");
    flow2ValueLabel->setFont(QFont("Arial", 15, QFont::Bold));
    flow2ValueLabel->setDefaultTextColor(Qt::black);
    flow2ValueLabel->setPos(750, 640);
    scene->addItem(flow2ValueLabel);

    // ========== 文字标注 ==========
    QFont bigFont("Arial", 22, QFont::Bold);
    auto addLabel = [&](const QString &text, const QPointF &pos) {
        auto *label = new QGraphicsTextItem(text);
        label->setFont(bigFont);
        label->setDefaultTextColor(Qt::black);
        label->setPos(pos);
        scene->addItem(label);
    };
    addLabel("驱动气入 ", QPointF(254, 190));
    addLabel("载气入", QPointF(261, 426));
    addLabel("样品入", QPointF(280, 902));
    addLabel("样品出", QPointF(270, 1055));
    addLabel("TCD载气出", QPointF(289, 1274));
    addLabel("TCD参比气出", QPointF(284, 1406));

    auto *valveLabel = new QGraphicsTextItem("电磁阀");
    valveLabel->setFont(QFont("Arial", 18, QFont::Bold));
    valveLabel->setDefaultTextColor(Qt::black);
    valveLabel->setPos(660, 280);
    scene->addItem(valveLabel);

    auto *fineLabel = new QGraphicsTextItem("微调阀");
    fineLabel->setFont(QFont("Arial", 18, QFont::Bold));
    fineLabel->setDefaultTextColor(Qt::black);
    fineLabel->setPos(917, 456);
    scene->addItem(fineLabel);

    auto *sixLabel = new QGraphicsTextItem("六通阀");
    sixLabel->setFont(QFont("Arial", 23, QFont::Bold));
    sixLabel->setDefaultTextColor(Qt::black);
    sixLabel->setPos(1050, 1012);
    scene->addItem(sixLabel);

    auto *ovenBigLabel = new QGraphicsTextItem("柱温箱");
    ovenBigLabel->setFont(QFont("Arial", 26, QFont::Bold));
    ovenBigLabel->setDefaultTextColor(Qt::black);
    ovenBigLabel->setPos(1510, 200);
    scene->addItem(ovenBigLabel);

    auto *detLabel = new QGraphicsTextItem("检测器");
    detLabel->setFont(QFont("Arial", 26, QFont::Bold));
    detLabel->setDefaultTextColor(Qt::black);
    detLabel->setPos(1900, 1248);
    scene->addItem(detLabel);

    // ========== 6个静态专业短管道 ==========
    bool pipeArrows[6] = {true, true, true, false, false, false};
    const double pipeLen = 150;
    QList<QPointF> pipePositions = {
        QPointF(100, 251),
        QPointF(160, 512),
        QPointF(160, 961),
        QPointF(171, 1102),
        QPointF(192, 1321),
        QPointF(192, 1365)
    };

    for (int i = 0; i < 6; ++i) {
        auto *pipeGroup = new QGraphicsItemGroup();
        pipeGroup->setFlag(QGraphicsItem::ItemIsMovable, false);
        scene->addItem(pipeGroup);

        double yCenter = 0;

        auto *flange = new QGraphicsRectItem(-6, -8, 12, 16, pipeGroup);
        flange->setBrush(QBrush(QColor("#b0b0b0")));
        flange->setPen(QPen(Qt::black, 1.5));

        auto *line1 = new QGraphicsLineItem(6, -3, pipeLen, -3, pipeGroup);
        line1->setPen(QPen(Qt::black, 2.5));
        auto *line2 = new QGraphicsLineItem(6, 3, pipeLen, 3, pipeGroup);
        line2->setPen(QPen(Qt::black, 2.5));

        QPolygonF arrow;
        if (pipeArrows[i]) {
            arrow << QPointF(pipeLen - 14, -12)
            << QPointF(pipeLen + 6, 0)
            << QPointF(pipeLen - 14, 12);
        } else {
            arrow << QPointF(20, -12)
            << QPointF(0, 0)
            << QPointF(20, 12);
        }
        auto *arrowItem = new QGraphicsPolygonItem(arrow, pipeGroup);
        arrowItem->setBrush(QBrush(Qt::black));
        arrowItem->setPen(QPen(Qt::black, 2));

        pipeGroup->setPos(pipePositions[i]);
    }

    // ========== 双击区域 ==========
    auto *flow1Rect = new DoubleClickableRect(QRectF(730, 340, 100, 100), "流量控制器1");
    scene->addItem(flow1Rect);
    QObject::connect(flow1Rect, &DoubleClickableRect::doubleClicked, ctrl, &ControlTab::showRealtimeDialog);

    auto *flow2Rect = new DoubleClickableRect(QRectF(730, 480, 100, 100), "流量控制器2");
    scene->addItem(flow2Rect);
    QObject::connect(flow2Rect, &DoubleClickableRect::doubleClicked, ctrl, &ControlTab::showRealtimeDialog);

    // 修正压力传感器双击区域
    auto *pressureRect = new DoubleClickableRect(QRectF(720, 900, 250, 250), "压力传感器");
    scene->addItem(pressureRect);
    QObject::connect(pressureRect, &DoubleClickableRect::doubleClicked, ctrl, &ControlTab::showRealtimeDialog);

    auto *tcdRect = new DoubleClickableRect(QRectF(1480, 1090, 415, 210), "TCD");
    scene->addItem(tcdRect);
    QObject::connect(tcdRect, &DoubleClickableRect::doubleClicked, ctrl, &ControlTab::showRealtimeDialog);

    auto *ovenRect = new DoubleClickableRect(QRectF(1210, 50, 300, 275), "柱温箱1");
    scene->addItem(ovenRect);
    QObject::connect(ovenRect, &DoubleClickableRect::doubleClicked, ctrl, &ControlTab::showRealtimeDialog);

    // ========== 连接阀门信号 ==========
    const auto items = scene->items();
    for (QGraphicsItem *item : items) {
        if (auto *valve = dynamic_cast<ValveItem*>(item)) {
            QObject::connect(valve, &ValveItem::clicked, ctrl, &ControlTab::handleValveClicked);
            QObject::connect(valve, &ValveItem::stateChanged, ctrl, [ctrl](const QString &name, bool state) {
                emit ctrl->commandRequested(name, state);
            });
        }
        if (auto *sixValve = dynamic_cast<SixWayValveItem*>(item)) {
            QObject::connect(sixValve, &SixWayValveItem::clicked, ctrl, &ControlTab::handleSixWayClicked);
            QObject::connect(sixValve, &SixWayValveItem::stateChanged, ctrl, [ctrl](const QString &name, bool state) {
                emit ctrl->commandRequested(name, state);
            });
        }
    }
}