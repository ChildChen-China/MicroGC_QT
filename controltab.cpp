#include "controltab.h"
#include "gassceneitems.h"
#include "communication.h"
#include "interactiveplot.h"
#include "qcustomplot.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsRectItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QBrush>
#include <QColor>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QtMath>
#include <QDebug>
#include <QApplication>
#include <QTimer>
#include <QDateTime>
#include <QSettings>

//==========================================================
// RealtimeDetailDialog
//==========================================================
RealtimeDetailDialog::RealtimeDetailDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
    , m_dialogType(0)
    , m_comm(nullptr)
    , m_autoScrollEnabled(true)
{
    setWindowTitle(title);
    resize(600, 500);

    auto *mainLayout = new QVBoxLayout(this);

    m_infoLayout = new QFormLayout;
    mainLayout->addLayout(m_infoLayout);

    m_plot = new InteractivePlot(this);
    m_plot->addGraph();
    m_plot->xAxis->setLabel("Time(s)");
    m_plot->yAxis->setLabel("Value");
    m_plot->setZoomMode(InteractivePlot::ZoomXY);
    m_plot->installEventFilter(this);
    mainLayout->addWidget(m_plot, 1);

    auto *btnLayout = new QHBoxLayout;
    m_clearBtn = new QPushButton("清除", this);
    m_restoreBtn = new QPushButton("还原", this);
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_restoreBtn);
    mainLayout->addLayout(btnLayout);

    auto *lengthLayout = new QHBoxLayout;
    lengthLayout->addWidget(new QLabel("显示长度:", this));
    m_lengthSpin = new QSpinBox(this);
    m_lengthSpin->setRange(10, 100000);
    lengthLayout->addWidget(m_lengthSpin);
    m_setLengthBtn = new QPushButton("设置", this);
    lengthLayout->addWidget(m_setLengthBtn);
    lengthLayout->addStretch();
    mainLayout->addLayout(lengthLayout);

    QSettings settings("MyCompany", "MicroGC");
    int savedLength = settings.value("realtime_length/" + title, 1000).toInt();
    m_lengthSpin->setValue(savedLength);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &RealtimeDetailDialog::updateFromComm);
    m_updateTimer->start(100);

    connect(m_clearBtn, &QPushButton::clicked, this, &RealtimeDetailDialog::clear);
    connect(m_restoreBtn, &QPushButton::clicked, this, &RealtimeDetailDialog::restoreView);
    connect(m_setLengthBtn, &QPushButton::clicked, this, [this, title]() {
        int len = m_lengthSpin->value();
        QSettings s("MyCompany", "MicroGC");
        s.setValue("realtime_length/" + title, len);
        s.sync();
        emit lengthSet(len);
    });

    m_plot->graph(0)->data()->clear();
    m_plot->replot();
}

void RealtimeDetailDialog::setCommunication(Communication *comm)
{
    m_comm = comm;
}

void RealtimeDetailDialog::updateFromComm()
{
    if (!m_comm || !m_comm->isConnected() || !m_autoScrollEnabled)
        return;

    static qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    double timeSec = (QDateTime::currentMSecsSinceEpoch() - startTime) / 1000.0;

    double value = 0;
    switch (m_dialogType) {
    case 0: value = m_comm->flow1(); break;
    case 1: value = m_comm->flow2(); break;
    case 2: value = m_comm->pressure(); break;
    case 3: value = m_comm->tcdMeasureTemp(); break;
    case 4: value = m_comm->columnOven1Temp(); break;
    default: return;
    }

    QVector<double> x(1), y(1);
    x[0] = timeSec;
    y[0] = value;
    m_plot->graph(0)->addData(x, y);
    m_plot->xAxis->setRange(timeSec - 20, timeSec);
    m_plot->yAxis->rescale(true);
    m_plot->replot();

    updateValueLabel(value);
}

bool RealtimeDetailDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_plot) {
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::Wheel) {
            m_autoScrollEnabled = false;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void RealtimeDetailDialog::clear()
{
    m_plot->graph(0)->data()->clear();
    m_plot->replot();
}

void RealtimeDetailDialog::restoreView()
{
    m_autoScrollEnabled = true;
    m_plot->rescaleAxes();
    m_plot->replot();
}

void RealtimeDetailDialog::addInfoRow(const QString &label, QWidget *widget)
{
    if (m_infoLayout)
        m_infoLayout->addRow(label, widget);
}

//==========================================================
// FlowControllerDialog（输入电压值，显示流量值）
//==========================================================
FlowControllerDialog::FlowControllerDialog(const QString &title, QWidget *parent)
    : RealtimeDetailDialog(title, parent)
{
    m_dialogType = (title == "流量控制器1") ? 0 : 1;

    auto *currentLabel = new QLabel("-- min/L", this);
    currentLabel->setStyleSheet("font-weight: bold; color: blue;");
    addInfoRow("当前流量值:", currentLabel);
    m_currentLabel = currentLabel;

    auto *setpointSpin = new QSpinBox(this);
    setpointSpin->setRange(1, 1000);          // 协议范围 1~1000
    m_setFlowBtn = new QPushButton("设置", this);
    QWidget *rowWidget = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0,0,0,0);
    rowLayout->addWidget(setpointSpin);
    rowLayout->addWidget(m_setFlowBtn);
    addInfoRow("设定电压:", rowWidget);

    connect(m_setFlowBtn, &QPushButton::clicked, this, [this, setpointSpin]() {
        int val = setpointSpin->value();
        if (m_comm) {
            if (m_dialogType == 0)
                m_comm->setFlow1Voltage(static_cast<quint16>(val));
            else
                m_comm->setFlow2Voltage(static_cast<quint16>(val));
        }
        emit flowVoltageSet(val);
    });
}

void FlowControllerDialog::updateValueLabel(double value)
{
    if (m_currentLabel)
        m_currentLabel->setText(QString("%1 min/L").arg(value, 0, 'f', 3));
}

//==========================================================
// PressureSensorDialog
//==========================================================
PressureSensorDialog::PressureSensorDialog(const QString &title, QWidget *parent)
    : RealtimeDetailDialog(title, parent)
{
    m_dialogType = 2;

    auto *currentLabel = new QLabel("-- KP", this);
    currentLabel->setStyleSheet("font-weight: bold; color: blue;");
    addInfoRow("当前压力:", currentLabel);
    m_currentLabel = currentLabel;

    auto *zeroBtn = new QPushButton("归零", this);
    addInfoRow("", zeroBtn);
    connect(zeroBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentLabel) m_currentLabel->setText("0.000 KP");
    });
}

void PressureSensorDialog::updateValueLabel(double value)
{
    if (m_currentLabel)
        m_currentLabel->setText(QString("%1 KP").arg(value, 0, 'f', 2));
}

//==========================================================
// TCDDialog（设置 TCD 温度）
//==========================================================
TCDDialog::TCDDialog(const QString &title, QWidget *parent)
    : RealtimeDetailDialog(title, parent)
{
    m_dialogType = 3;

    auto *currentLabel = new QLabel("-- ℃", this);
    currentLabel->setStyleSheet("font-weight: bold; color: blue;");
    addInfoRow("测量温度LL:", currentLabel);
    m_currentLabel = currentLabel;

    auto *setTempSpin = new QSpinBox(this);
    setTempSpin->setRange(0, 500);
    setTempSpin->setSuffix(" ℃");

    QPushButton *setBtn = new QPushButton("设置", this);
    setBtn->setToolTip("TS 98\r  设置TCD温度");
    QWidget *rowWidget = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0,0,0,0);
    rowLayout->addWidget(setTempSpin);
    rowLayout->addWidget(setBtn);
    addInfoRow("设定温度:", rowWidget);

    connect(setBtn, &QPushButton::clicked, this, [this, setTempSpin]() {
        if (m_comm)
            m_comm->setTcdTemperature(static_cast<quint16>(setTempSpin->value()));
    });
}

void TCDDialog::updateValueLabel(double value)
{
    if (m_currentLabel)
        m_currentLabel->setText(QString("%1 ℃").arg(value, 0, 'f', 2));
}

//==========================================================
// ColumnOvenDialog（设置柱温箱温度）
//==========================================================
ColumnOvenDialog::ColumnOvenDialog(const QString &title, QWidget *parent)
    : RealtimeDetailDialog(title, parent)
{
    m_dialogType = 4;

    auto *currentLabel = new QLabel("-- ℃", this);
    currentLabel->setStyleSheet("font-weight: bold; color: blue;");
    addInfoRow("当前温度:", currentLabel);
    m_currentLabel = currentLabel;

    auto *setTempSpin = new QSpinBox(this);
    setTempSpin->setRange(1, 1000);
    setTempSpin->setSuffix(" ℃");

    m_setTempBtn = new QPushButton("设置", this);
    QWidget *rowWidget = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0,0,0,0);
    rowLayout->addWidget(setTempSpin);
    rowLayout->addWidget(m_setTempBtn);
    addInfoRow("设定温度:", rowWidget);

    connect(m_setTempBtn, &QPushButton::clicked, this, [this, setTempSpin]() {
        int val = setTempSpin->value();
        if (!m_comm) return;

        m_comm->requestRegisterRead(0x0400, [this, val](quint16 ovenStatus) {
            if (ovenStatus == 1) {
                m_comm->setColumnOven1Temperature(static_cast<quint16>(val));
            } else {
                m_comm->setColumnOvenEnable(true);
                QTimer::singleShot(200, this, [this, val]() {
                    m_comm->setColumnOven1Temperature(static_cast<quint16>(val));
                });
            }
        });

        emit tempSetpointSet(val);
    });
}

void ColumnOvenDialog::updateValueLabel(double value)
{
    if (m_currentLabel)
        m_currentLabel->setText(QString("%1 ℃").arg(value, 0, 'f', 2));
}

//==========================================================
// ControlTab
//==========================================================
ControlTab::ControlTab(QWidget *parent)
    : QWidget(parent), m_view(nullptr), m_scene(nullptr), m_comm(nullptr)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 2006, 1463);
    m_scene->setBackgroundBrush(Qt::white);

    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setRenderHint(QPainter::TextAntialiasing, true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    buildGasScene(m_scene, m_comm, this);

    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void ControlTab::setCommunication(Communication *comm)
{
    m_comm = comm;
    if (m_comm) {
        connect(m_comm, &Communication::slowDataUpdated, this, &ControlTab::updateRealtimeLabels);
        updateRealtimeLabels();
    }
}

void ControlTab::handleValveClicked(ValveItem *valve)
{
    if (!valve) return;

    if (valve->name() != "电磁阀")
        return;

    bool newState = !valve->state();
    valve->setState(newState);

    const auto items = m_scene->items();
    for (QGraphicsItem *item : items) {
        if (auto *six = dynamic_cast<SixWayValveItem*>(item)) {
            six->setState(newState);
        }
    }

    if (m_comm) {
        m_comm->setValveBit(0, newState);
        m_comm->setSixWayValve1(newState);
    }

    emit commandRequested("电磁阀", newState);
    emit logMessage("控制", QString("电磁阀状态: %1，六通阀同步切换").arg(newState ? "开启" : "关闭"));
    emit sixWayValveToggled();
}

void ControlTab::handleSixWayClicked(SixWayValveItem *clickedValve)
{
    Q_UNUSED(clickedValve);
}

void ControlTab::showRealtimeDialog(const QString &title)
{
    RealtimeDetailDialog *dlg = nullptr;

    if (title == "流量控制器1" || title == "流量控制器2") {
        auto *fdlg = new FlowControllerDialog(title, this);
        fdlg->setCommunication(m_comm);
        connect(fdlg, &RealtimeDetailDialog::lengthSet, this, [this, title](int len) {
            if (title == "流量控制器1") emit displayLengthChanged("Flow1", len);
            else emit displayLengthChanged("Flow2", len);
        });
        dlg = fdlg;
    } else if (title == "压力传感器") {
        auto *pdlg = new PressureSensorDialog(title, this);
        pdlg->setCommunication(m_comm);
        connect(pdlg, &RealtimeDetailDialog::lengthSet, this, [this](int len) {
            emit displayLengthChanged("Pressure", len);
        });
        dlg = pdlg;
    } else if (title == "TCD") {
        auto *tdlg = new TCDDialog(title, this);
        tdlg->setCommunication(m_comm);
        connect(tdlg, &RealtimeDetailDialog::lengthSet, this, [this](int len) {
            emit displayLengthChanged("TCD", len);
        });
        dlg = tdlg;
    } else if (title == "柱温箱1" || title == "柱温箱2") {
        auto *odlg = new ColumnOvenDialog("柱温箱", this);
        odlg->setCommunication(m_comm);
        connect(odlg, &RealtimeDetailDialog::lengthSet, this, [this](int len) {
            emit displayLengthChanged("Oven", len);
        });
        dlg = odlg;
    }

    if (dlg) {
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }
}

void ControlTab::updateRealtimeLabels()
{
    if (!m_comm) return;

    const auto items = m_scene->items();
    for (QGraphicsItem *item : items) {
        if (auto *text = qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            QString name = text->objectName();
            if (name == "ovenTempLabel") {
                text->setPlainText(QString::number(m_comm->columnOven1Temp()) + " ℃");
            } else if (name == "tcdTempLabel") {
                text->setPlainText(QString::number(m_comm->tcdMeasureTemp()) + " ℃");
            } else if (name == "pressureValueLabel") {
                text->setPlainText(QString::number(m_comm->pressure()) + " KP");
            } else if (name == "flow1ValueLabel") {
                text->setPlainText(QString::number(m_comm->flow1()) + " min/L");
            } else if (name == "flow2ValueLabel") {
                text->setPlainText(QString::number(m_comm->flow2()) + " min/L");
            }
        }
    }
}

void ControlTab::updateDeviceState(const QString &device, bool state)
{
    const auto items = m_scene->items();
    for (QGraphicsItem *item : items) {
        if (auto *valve = dynamic_cast<ValveItem*>(item)) {
            if (valve->name() == device) valve->setState(state);
        } else if (auto *sixValve = dynamic_cast<SixWayValveItem*>(item)) {
            if (sixValve->name() == device) sixValve->setState(state);
        }
    }
}

void ControlTab::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_view && m_scene) m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}