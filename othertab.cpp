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
#include <QScrollArea>

OtherTab::OtherTab(QWidget *parent)
    : QWidget(parent)
    , m_resetBtn(nullptr)
    , m_chkColumnOven(nullptr)
    , m_chkPressure(nullptr)
    , m_chkTcdTemp(nullptr)
    , m_chkFlow1(nullptr)
    , m_chkFlow2(nullptr)
    , m_zoomGroup(nullptr)
    , m_crosshairCheck(nullptr)
    , m_plotColumnOven(nullptr)
    , m_plotPressure(nullptr)
    , m_plotTcdTemp(nullptr)
    , m_plotFlow1(nullptr)
    , m_plotFlow2(nullptr)
    , m_groupColumnOven(nullptr)
    , m_groupPressure(nullptr)
    , m_groupTcdTemp(nullptr)
    , m_groupFlow1(nullptr)
    , m_groupFlow2(nullptr)
    , m_comm(nullptr)
    , m_autoScrollEnabled(true)
    , m_scrollTimer(new QTimer(this))
    , m_startTime(QDateTime::currentMSecsSinceEpoch())
    , m_topRowLayout(nullptr)
    , m_bottomRowLayout(nullptr)
    , m_plotLayout(nullptr)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // 顶部控件行 + 图表区域，用 QScrollArea 包裹
    auto *content = new QWidget();
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 4, 4);
    contentLayout->addLayout(setupFirstRow());
    contentLayout->addLayout(setupPlotLayout(), 1);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scroll);

    m_chkColumnOven->setChecked(true);
    m_chkPressure->setChecked(true);
    m_chkTcdTemp->setChecked(true);
    m_chkFlow1->setChecked(true);
    m_chkFlow2->setChecked(true);
    toggleSensorVisible();

    QList<InteractivePlot*> plots = {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                      m_plotFlow1, m_plotFlow2};
    for (InteractivePlot *plot : plots) {
        if (plot) plot->installEventFilter(this);
    }

    m_scrollTimer->setInterval(100);
    connect(m_scrollTimer, &QTimer::timeout, this, &OtherTab::scrollPlots);
    m_scrollTimer->start();
}

void OtherTab::setCommunication(Communication *comm)
{
    m_comm = comm;
    if (m_comm) {
        connect(m_comm, &Communication::slowDataUpdated, this, &OtherTab::updateFromComm);
        connect(m_comm, &Communication::connected, this, [this]() {
            m_startTime = QDateTime::currentMSecsSinceEpoch();
        });
    }
}

QHBoxLayout* OtherTab::setupFirstRow()
{
    auto *layout = new QHBoxLayout;

    m_chkColumnOven = new QCheckBox("TC4温度", this);
    m_chkPressure   = new QCheckBox("压力传感器", this);
    m_chkTcdTemp    = new QCheckBox("TCD", this);
    m_chkFlow1      = new QCheckBox("载气", this);
    m_chkFlow2      = new QCheckBox("参比气", this);

    layout->addWidget(m_chkColumnOven);
    layout->addWidget(m_chkPressure);
    layout->addWidget(m_chkTcdTemp);
    layout->addWidget(m_chkFlow1);
    layout->addWidget(m_chkFlow2);

    layout->addSpacing(10);

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

    m_crosshairCheck = new QCheckBox("十字标", this);
    layout->addWidget(m_crosshairCheck);

    m_resetBtn = new QPushButton("还原", this);
    layout->addWidget(m_resetBtn);
    connect(m_resetBtn, &QPushButton::clicked, this, &OtherTab::resetAllPlots);

    layout->addStretch();

    connect(m_zoomGroup, qOverload<int>(&QButtonGroup::idClicked),
            this, &OtherTab::applyZoomMode);
    connect(m_crosshairCheck, &QCheckBox::toggled,
            this, &OtherTab::setCrosshairEnabled);

    connect(m_chkColumnOven, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkPressure, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkTcdTemp, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkFlow1, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);
    connect(m_chkFlow2, &QCheckBox::toggled, this, &OtherTab::toggleSensorVisible);

    return layout;
}

QVBoxLayout* OtherTab::setupPlotLayout()
{
    m_plotLayout = new QVBoxLayout;
    m_plotLayout->setSpacing(6);

    // 第一行
    m_topRowLayout = new QHBoxLayout;
    m_topRowLayout->setSpacing(6);

    // TC4 温度
    m_groupColumnOven = new QGroupBox("TC4温度", this);
    m_groupColumnOven->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto *l = new QVBoxLayout(m_groupColumnOven);
        m_plotColumnOven = new InteractivePlot(m_groupColumnOven);
        m_plotColumnOven->addGraph();
        m_plotColumnOven->graph(0)->setPen(QPen(Qt::blue));
        m_plotColumnOven->xAxis->setLabel("Time(s)");
        m_plotColumnOven->yAxis->setLabel("Temperature(°C)");
        m_plotColumnOven->setCrosshairName("TC4");
        l->addWidget(m_plotColumnOven);
    }
    m_topRowLayout->addWidget(m_groupColumnOven, 1);

    // 压力
    m_groupPressure = new QGroupBox("压力传感器", this);
    m_groupPressure->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto *l = new QVBoxLayout(m_groupPressure);
        m_plotPressure = new InteractivePlot(m_groupPressure);
        m_plotPressure->addGraph();
        m_plotPressure->graph(0)->setPen(QPen(Qt::darkGreen));
        m_plotPressure->xAxis->setLabel("Time(s)");
        m_plotPressure->yAxis->setLabel("Pressure(kPa)");
        m_plotPressure->setCrosshairName("压力传感器");
        l->addWidget(m_plotPressure);
    }
    m_topRowLayout->addWidget(m_groupPressure, 1);

    // TCD：三条曲线
    m_groupTcdTemp = new QGroupBox("TCD", this);
    m_groupTcdTemp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto *l = new QVBoxLayout(m_groupTcdTemp);
        m_plotTcdTemp = new InteractivePlot(m_groupTcdTemp);

        m_plotTcdTemp->addGraph();
        m_plotTcdTemp->graph(0)->setName("FA功率(%)");
        m_plotTcdTemp->graph(0)->setPen(QPen(Qt::red));

        m_plotTcdTemp->addGraph();
        m_plotTcdTemp->graph(1)->setName("FB功率(%)");
        m_plotTcdTemp->graph(1)->setPen(QPen(Qt::blue));

        m_plotTcdTemp->addGraph();
        m_plotTcdTemp->graph(2)->setName("实测温度(°C)");
        m_plotTcdTemp->graph(2)->setPen(QPen(Qt::darkYellow));

        m_plotTcdTemp->xAxis->setLabel("Time(s)");
        m_plotTcdTemp->yAxis->setLabel("Value");
        m_plotTcdTemp->setCrosshairName("TCD");
        m_plotTcdTemp->legend->setVisible(true);
        m_plotTcdTemp->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);
        l->addWidget(m_plotTcdTemp);
    }
    m_topRowLayout->addWidget(m_groupTcdTemp, 1);

    // 第二行
    m_bottomRowLayout = new QHBoxLayout;
    m_bottomRowLayout->setSpacing(6);

    // 载气图表（原流量器1）
    m_groupFlow1 = new QGroupBox("载气", this);
    m_groupFlow1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto *l = new QVBoxLayout(m_groupFlow1);
        m_plotFlow1 = new InteractivePlot(m_groupFlow1);
        m_plotFlow1->addGraph();
        m_plotFlow1->graph(0)->setPen(QPen(Qt::magenta));
        m_plotFlow1->xAxis->setLabel("Time(s)");
        m_plotFlow1->yAxis->setLabel("psi");
        m_plotFlow1->setCrosshairName("载气");
        l->addWidget(m_plotFlow1);
    }
    m_bottomRowLayout->addWidget(m_groupFlow1, 1);   // ← 从 top 改到 bottom

    // 参比气图表（原流量器2）
    m_groupFlow2 = new QGroupBox("参比气", this);
    m_groupFlow2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        auto *l = new QVBoxLayout(m_groupFlow2);
        m_plotFlow2 = new InteractivePlot(m_groupFlow2);
        m_plotFlow2->addGraph();
        m_plotFlow2->graph(0)->setPen(QPen(Qt::cyan));
        m_plotFlow2->xAxis->setLabel("Time(s)");
        m_plotFlow2->yAxis->setLabel("psi");
        m_plotFlow2->setCrosshairName("参比气");
        l->addWidget(m_plotFlow2);
    }
    m_bottomRowLayout->addWidget(m_groupFlow2, 1);


    m_plotLayout->addLayout(m_topRowLayout, 1);
    m_plotLayout->addLayout(m_bottomRowLayout, 1);

    return m_plotLayout;
}

void OtherTab::updateFromComm()
{
    if (!m_comm || !m_comm->isConnected())
        return;
    if (QApplication::mouseButtons() & Qt::LeftButton)
        return;

    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;

    auto updatePlot = [this, timeSec](InteractivePlot* plot, int graphIdx, double value, int maxPoints) {
        if (!plot || plot->graphCount() <= graphIdx) return;
        QCPGraph *graph = plot->graph(graphIdx);
        graph->addData(timeSec, value);
        if (maxPoints > 0 && graph->dataCount() > maxPoints) {
            int excess = graph->dataCount() - maxPoints;
            for (int i = 0; i < excess; ++i)
                graph->data()->remove(graph->data()->begin()->key);
        }
    };

    // TC4 温度（原柱温箱）
    updatePlot(m_plotColumnOven, 0, m_comm->columnOven1Temp(), m_columnOvenLength);

    // 压力：原始 0.1 kPa/计数 → 除以 10 得到 kPa
    updatePlot(m_plotPressure, 0, m_comm->pressure() / 10.0, m_pressureLength);

    // TCD 三条曲线
    updatePlot(m_plotTcdTemp, 0, m_comm->tcdPowerFA(), m_tcdTempLength);
    updatePlot(m_plotTcdTemp, 1, m_comm->tcdPowerFB(), m_tcdTempLength);
    updatePlot(m_plotTcdTemp, 2, m_comm->tcdMeasureTemp(), m_tcdTempLength);

    // 载气、参比气转换为 psi 后绘图
    double flow1Psi = m_comm->flow1() * 20.0 - 0.008;
    double flow2Psi = m_comm->flow2() * 20.0 - 0.008;
    if (flow1Psi < 0.0) flow1Psi = 0.0;   // ★
    if (flow2Psi < 0.0) flow2Psi = 0.0;   // ★
    updatePlot(m_plotFlow1, 0, flow1Psi, m_flow1Length);
    updatePlot(m_plotFlow2, 0, flow2Psi, m_flow2Length);


    if (m_autoScrollEnabled) {
        if (m_plotColumnOven->isVisible()) m_plotColumnOven->yAxis->rescale(true);
        if (m_plotPressure->isVisible())   m_plotPressure->yAxis->rescale(true);
        if (m_plotTcdTemp->isVisible())    m_plotTcdTemp->yAxis->rescale(true);
        if (m_plotFlow1->isVisible())      m_plotFlow1->yAxis->rescale(true);
        if (m_plotFlow2->isVisible())      m_plotFlow2->yAxis->rescale(true);
    }

    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot && plot->isVisible())
            plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void OtherTab::scrollPlots()
{
    if (!m_comm || !m_comm->isConnected() || !m_autoScrollEnabled)
        return;

    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot && plot->isVisible()) {
            plot->xAxis->setRange(timeSec - 20, timeSec);
            plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }
}

void OtherTab::resetAllPlots()
{
    m_autoScrollEnabled = true;
    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) {
            plot->xAxis->setRange(timeSec - 20, timeSec);
            plot->yAxis->rescale(true);
            plot->replot();
        }
    }
}

bool OtherTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Wheel)
        m_autoScrollEnabled = false;
    return QWidget::eventFilter(watched, event);
}

void OtherTab::applyZoomMode()
{
    int id = m_zoomGroup->checkedId();
    auto mode = static_cast<InteractivePlot::ZoomMode>(id);
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) plot->setZoomMode(mode);
    }
}

void OtherTab::setCrosshairEnabled(bool enabled)
{
    for (InteractivePlot *plot : {m_plotColumnOven, m_plotPressure, m_plotTcdTemp,
                                  m_plotFlow1, m_plotFlow2}) {
        if (plot) plot->setCrosshairEnabled(enabled);
    }
}

void OtherTab::toggleSensorVisible()
{
    m_groupColumnOven->setVisible(m_chkColumnOven->isChecked());
    m_groupPressure->setVisible(m_chkPressure->isChecked());
    m_groupTcdTemp->setVisible(m_chkTcdTemp->isChecked());
    m_groupFlow1->setVisible(m_chkFlow1->isChecked());
    m_groupFlow2->setVisible(m_chkFlow2->isChecked());

    int topVisible = 0;
    if (m_groupColumnOven->isVisible()) topVisible++;
    if (m_groupPressure->isVisible()) topVisible++;
    if (m_groupTcdTemp->isVisible()) topVisible++;

    int bottomVisible = 0;
    if (m_groupFlow1->isVisible()) bottomVisible++;
    if (m_groupFlow2->isVisible()) bottomVisible++;

    if (m_plotLayout) {
        if (topVisible == 0 && bottomVisible > 0) {
            m_plotLayout->setStretch(0, 0);
            m_plotLayout->setStretch(1, 1);
        } else if (bottomVisible == 0 && topVisible > 0) {
            m_plotLayout->setStretch(0, 1);
            m_plotLayout->setStretch(1, 0);
        } else {
            m_plotLayout->setStretch(0, 1);
            m_plotLayout->setStretch(1, 1);
        }
    }
}

void OtherTab::setColumnOvenLength(int points) { m_columnOvenLength = points; }
void OtherTab::setPressureLength(int points) { m_pressureLength = points; }
void OtherTab::setTcdTempLength(int points) { m_tcdTempLength = points; }
void OtherTab::setFlow1Length(int points) { m_flow1Length = points; }
void OtherTab::setFlow2Length(int points) { m_flow2Length = points; }