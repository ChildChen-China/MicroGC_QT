#include "othertab.h"
#include "communication.h"
#include "interactiveplot.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QPen>
#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDateTime>
#include <QDebug>

OtherTab::OtherTab(QWidget *parent)
    : QWidget(parent)
    , m_resetBtn(nullptr)
    , m_chkColumnOven(nullptr)
    , m_chkPressure(nullptr)
    , m_chkVacuum(nullptr)
    , m_chkFlow1(nullptr)
    , m_chkFlow2(nullptr)
    , m_zoomGroup(nullptr)
    , m_crosshairCheck(nullptr)
    , m_plotColumnOven(nullptr)
    , m_plotPressure(nullptr)
    , m_plotVacuum(nullptr)
    , m_plotFlow1(nullptr)
    , m_plotFlow2(nullptr)
    , m_groupColumnOven(nullptr)
    , m_groupPressure(nullptr)
    , m_groupVacuum(nullptr)
    , m_groupFlow1(nullptr)
    , m_groupFlow2(nullptr)
    , m_comm(nullptr)
    , m_autoScrollEnabled(true),
    m_scrollTimer(new QTimer(this)),
    m_startTime(QDateTime::currentMSecsSinceEpoch())
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(setupFirstRow());
    mainLayout->addLayout(setupSecondRow(), 1);
    mainLayout->addLayout(setupThirdRow(), 1);

    // 默认全部显示
    m_chkColumnOven->setChecked(true);
    m_chkPressure->setChecked(true);
    m_chkVacuum->setChecked(true);
    m_chkFlow1->setChecked(true);
    m_chkFlow2->setChecked(true);

    toggleSensorVisible();

    // 安装事件过滤器，捕获鼠标操作
    QList<InteractivePlot*> plots = {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                      m_plotFlow1, m_plotFlow2};
    for (InteractivePlot *plot : plots) {
        if (plot) {
            plot->installEventFilter(this);
        }
    }
    // 创建滚动定时器，100ms触发一次
    m_scrollTimer->setInterval(100);
    connect(m_scrollTimer, &QTimer::timeout, this, &OtherTab::scrollPlots);
    m_scrollTimer->start();
}

void OtherTab::scrollPlots()
{
    // 未连接或不允许自动滚动时，不更新X轴
    if (!m_comm || !m_comm->isConnected() || !m_autoScrollEnabled)
        return;

    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;

    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot && plot->isVisible()) {
            plot->xAxis->setRange(timeSec - 20, timeSec);
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void OtherTab::setCommunication(Communication *comm)
{
    m_comm = comm;
    if (m_comm) {
        connect(m_comm, &Communication::slowDataUpdated, this, &OtherTab::updateFromComm);
        // 连接成功时重置起始时间，避免显示未连接期间的空白
        connect(m_comm, &Communication::connected, this, [this]() {
            m_startTime = QDateTime::currentMSecsSinceEpoch();
        });
    }
}

QHBoxLayout* OtherTab::setupFirstRow()
{
    auto *layout = new QHBoxLayout;

    // 传感器显示选择
    m_chkColumnOven = new QCheckBox("柱温箱", this);
    m_chkPressure = new QCheckBox("压力传感器", this);
    m_chkVacuum = new QCheckBox("真空规", this);
    m_chkFlow1 = new QCheckBox("流量器1", this);
    m_chkFlow2 = new QCheckBox("流量器2", this);

    layout->addWidget(m_chkColumnOven);
    layout->addWidget(m_chkPressure);
    layout->addWidget(m_chkVacuum);
    layout->addWidget(m_chkFlow1);
    layout->addWidget(m_chkFlow2);

    layout->addSpacing(10);

    // 缩放模式
    QRadioButton *zoomX = new QRadioButton("X轴缩放", this);
    QRadioButton *zoomY = new QRadioButton("Y轴缩放", this);
    QRadioButton *zoomXY = new QRadioButton("XY缩放", this);
    QRadioButton *zoomRect = new QRadioButton("框选放大", this);
    zoomXY->setChecked(true);

    m_zoomGroup = new QButtonGroup(this);
    m_zoomGroup->addButton(zoomX, static_cast<int>(InteractivePlot::ZoomX));
    m_zoomGroup->addButton(zoomY, static_cast<int>(InteractivePlot::ZoomY));
    m_zoomGroup->addButton(zoomXY, static_cast<int>(InteractivePlot::ZoomXY));
    m_zoomGroup->addButton(zoomRect, static_cast<int>(InteractivePlot::ZoomRect));

    layout->addWidget(zoomX);
    layout->addWidget(zoomY);
    layout->addWidget(zoomXY);
    layout->addWidget(zoomRect);

    // 十字标
    m_crosshairCheck = new QCheckBox("十字标", this);
    layout->addWidget(m_crosshairCheck);

    // 还原按钮
    m_resetBtn = new QPushButton("还原", this);
    layout->addWidget(m_resetBtn);
    connect(m_resetBtn, &QPushButton::clicked, this, &OtherTab::resetAllPlots);

    layout->addStretch();

    // 连接信号
    connect(m_zoomGroup, qOverload<int>(&QButtonGroup::idClicked),
            this, &OtherTab::applyZoomMode);
    connect(m_crosshairCheck, &QCheckBox::toggled,
            this, &OtherTab::setCrosshairEnabled);

    connect(m_chkColumnOven, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkPressure, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkVacuum, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkFlow1, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkFlow2, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);

    return layout;
}

QHBoxLayout* OtherTab::setupSecondRow()
{
    auto *layout = new QHBoxLayout;

    // 柱温箱
    m_groupColumnOven = new QGroupBox("柱温箱", this);
    m_groupColumnOven->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *coLayout = new QVBoxLayout(m_groupColumnOven);
    m_plotColumnOven = new InteractivePlot(m_groupColumnOven);
    m_plotColumnOven->addGraph();   // graph 0
    m_plotColumnOven->graph(0)->setPen(QPen(Qt::blue));
    m_plotColumnOven->xAxis->setLabel("Time(s)");
    m_plotColumnOven->yAxis->setLabel("Temperature(°C)");
    m_plotColumnOven->setCrosshairName("柱温箱");
    coLayout->addWidget(m_plotColumnOven);
    layout->addWidget(m_groupColumnOven, 1);

    // 压力传感器
    m_groupPressure = new QGroupBox("压力传感器", this);
    m_groupPressure->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *pLayout = new QVBoxLayout(m_groupPressure);
    m_plotPressure = new InteractivePlot(m_groupPressure);
    m_plotPressure->addGraph();
    m_plotPressure->graph(0)->setPen(QPen(Qt::darkGreen));
    m_plotPressure->xAxis->setLabel("Time(s)");
    m_plotPressure->yAxis->setLabel("Pressure(kPa)");
    m_plotPressure->setCrosshairName("压力传感器");
    pLayout->addWidget(m_plotPressure);
    layout->addWidget(m_groupPressure, 1);

    // 真空规（暂无数据，仅显示空曲线）
    m_groupVacuum = new QGroupBox("真空规", this);
    m_groupVacuum->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *vLayout = new QVBoxLayout(m_groupVacuum);
    m_plotVacuum = new InteractivePlot(m_groupVacuum);
    m_plotVacuum->addGraph();
    m_plotVacuum->graph(0)->setPen(QPen(Qt::darkYellow));
    m_plotVacuum->xAxis->setLabel("Time(s)");
    m_plotVacuum->yAxis->setLabel("Pressure(Pa)");
    m_plotVacuum->setCrosshairName("真空规");
    vLayout->addWidget(m_plotVacuum);
    layout->addWidget(m_groupVacuum, 1);

    return layout;
}

QHBoxLayout* OtherTab::setupThirdRow()
{
    auto *layout = new QHBoxLayout;

    // 流量器1
    m_groupFlow1 = new QGroupBox("流量器1", this);
    m_groupFlow1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *f1Layout = new QVBoxLayout(m_groupFlow1);
    m_plotFlow1 = new InteractivePlot(m_groupFlow1);
    m_plotFlow1->addGraph();
    m_plotFlow1->graph(0)->setPen(QPen(Qt::magenta));
    m_plotFlow1->xAxis->setLabel("Time(s)");
    m_plotFlow1->yAxis->setLabel("Flow(ml/min)");
    m_plotFlow1->setCrosshairName("流量器1");
    f1Layout->addWidget(m_plotFlow1);
    layout->addWidget(m_groupFlow1, 1);

    // 流量器2
    m_groupFlow2 = new QGroupBox("流量器2", this);
    m_groupFlow2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *f2Layout = new QVBoxLayout(m_groupFlow2);
    m_plotFlow2 = new InteractivePlot(m_groupFlow2);
    m_plotFlow2->addGraph();
    m_plotFlow2->graph(0)->setPen(QPen(Qt::cyan));
    m_plotFlow2->xAxis->setLabel("Time(s)");
    m_plotFlow2->yAxis->setLabel("Flow(ml/min)");
    m_plotFlow2->setCrosshairName("流量器2");
    f2Layout->addWidget(m_plotFlow2);
    layout->addWidget(m_groupFlow2, 1);

    return layout;
}

void OtherTab::updateFromComm()
{
    if (!m_comm || !m_comm->isConnected())
        return;

    if (QApplication::mouseButtons() & Qt::LeftButton)
        return; // 用户交互时不更新数据（可选，根据需要调整）

    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;

    // 柱温箱温度
    if (m_plotColumnOven && m_plotColumnOven->graphCount() > 0) {
        m_plotColumnOven->graph(0)->addData(timeSec, m_comm->columnOven1Temp());
        if (m_autoScrollEnabled) m_plotColumnOven->yAxis->rescale(true);
    }

    // 压力传感器
    if (m_plotPressure && m_plotPressure->graphCount() > 0) {
        m_plotPressure->graph(0)->addData(timeSec, m_comm->pressure());
        if (m_autoScrollEnabled) m_plotPressure->yAxis->rescale(true);
    }

    // 真空规
    if (m_plotVacuum && m_plotVacuum->graphCount() > 0) {
        m_plotVacuum->graph(0)->addData(timeSec, 0.0);
        if (m_autoScrollEnabled) m_plotVacuum->yAxis->rescale(true);
    }

    // 流量器1
    if (m_plotFlow1 && m_plotFlow1->graphCount() > 0) {
        m_plotFlow1->graph(0)->addData(timeSec, m_comm->flow1());
        if (m_autoScrollEnabled) m_plotFlow1->yAxis->rescale(true);
    }

    // 流量器2
    if (m_plotFlow2 && m_plotFlow2->graphCount() > 0) {
        m_plotFlow2->graph(0)->addData(timeSec, m_comm->flow2());
        if (m_autoScrollEnabled) m_plotFlow2->yAxis->rescale(true);
    }

    // 统一重绘（延迟重绘，减少负担）
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot && plot->isVisible()) {
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void OtherTab::resetAllPlots()
{
    m_autoScrollEnabled = true;
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) {
            plot->rescaleAxes();
            plot->replot();
        }
    }
}

bool OtherTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::Wheel) {
        m_autoScrollEnabled = false;
    }
    return QWidget::eventFilter(watched, event);
}

void OtherTab::applyZoomMode()
{
    int id = m_zoomGroup->checkedId();
    auto mode = static_cast<InteractivePlot::ZoomMode>(id);
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) plot->setZoomMode(mode);
    }
}

void OtherTab::setCrosshairEnabled(bool enabled)
{
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotVacuum,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) plot->setCrosshairEnabled(enabled);
    }
}

void OtherTab::toggleSensorVisible()
{
    m_groupColumnOven->setVisible(m_chkColumnOven->isChecked());
    m_groupPressure->setVisible(m_chkPressure->isChecked());
    m_groupVacuum->setVisible(m_chkVacuum->isChecked());
    m_groupFlow1->setVisible(m_chkFlow1->isChecked());
    m_groupFlow2->setVisible(m_chkFlow2->isChecked());
}
