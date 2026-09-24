#include "monitortab.h"
#include "communication.h"
#include "interactiveplot.h"
#include "qcustomplot.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QFileDialog>
#include <QPen>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QtMath>
#include <QSettings>

//==========================================================
// SignalPlotPanel 实现
//==========================================================
SignalPlotPanel::SignalPlotPanel(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
    , m_plot(new InteractivePlot(this))
    , m_pauseBtn(new QPushButton("暂停", this))
    , m_invertBtn(new QPushButton("反转", this))
    , m_resetBtn(new QPushButton("重置", this))
    , m_crosshairBtn(new QPushButton("十字标", this))
    , m_filterBtn(new QPushButton("显示滤波前", this))
    , m_zoomGroup(new QButtonGroup(this))
    , m_inverted(false)
    , m_hasSnapshot(false)
{
    m_invertBtn->setCheckable(true);
    m_pauseBtn->setCheckable(true);
    m_crosshairBtn->setCheckable(true);

    QPushButton *zoomXBtn = new QPushButton("X轴缩放", this);
    QPushButton *zoomYBtn = new QPushButton("Y轴缩放", this);
    QPushButton *zoomXYBtn = new QPushButton("XY缩放", this);
    QPushButton *zoomRectBtn = new QPushButton("框选放大", this);

    for (QPushButton *btn : {zoomXBtn, zoomYBtn, zoomXYBtn, zoomRectBtn}) {
        btn->setCheckable(true);
        m_zoomGroup->addButton(btn);
    }
    zoomXYBtn->setChecked(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_plot->xAxis->setLabel("Time(s)");
    m_plot->yAxis->setLabel("Amplitude(mV)");
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(Qt::blue));
    m_plot->setCrosshairName(title);

    layout->addWidget(m_plot);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_invertBtn);
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addWidget(m_crosshairBtn);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(zoomXBtn);
    btnLayout->addWidget(zoomYBtn);
    btnLayout->addWidget(zoomXYBtn);
    btnLayout->addWidget(zoomRectBtn);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(m_filterBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_pauseBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_pauseBtn->setText(checked ? "继续" : "暂停");
        m_plot->setInteraction(QCP::iRangeDrag, !checked);
        m_plot->setInteraction(QCP::iRangeZoom, !checked);
        emit pauseStateChanged(checked);
    });

    connect(m_invertBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_inverted = checked;
        m_invertBtn->setText(checked ? "恢复" : "反转");
        applyInversion();
        m_plot->replot();
    });

    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        m_inverted = false;
        m_invertBtn->setChecked(false);
        m_invertBtn->setText("反转");

        // 拉回最新数据：X 轴显示最后 20 秒，Y 轴自适应
        if (!m_x.isEmpty()) {
            double lastX = m_x.last();
            const double rangeSpan = 20.0;
            if (lastX <= rangeSpan) {
                m_plot->xAxis->setRange(0, rangeSpan);
            } else {
                m_plot->xAxis->setRange(lastX - rangeSpan, lastX);
            }
            m_plot->yAxis->rescale(true);
        }
        m_plot->replot();
    });

    connect(m_crosshairBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_plot->setCrosshairEnabled(checked);
    });

    connect(zoomXBtn, &QPushButton::clicked, this, [this]() { m_plot->setZoomMode(InteractivePlot::ZoomX); });
    connect(zoomYBtn, &QPushButton::clicked, this, [this]() { m_plot->setZoomMode(InteractivePlot::ZoomY); });
    connect(zoomXYBtn, &QPushButton::clicked, this, [this]() { m_plot->setZoomMode(InteractivePlot::ZoomXY); });
    connect(zoomRectBtn, &QPushButton::clicked, this, [this]() { m_plot->setZoomMode(InteractivePlot::ZoomRect); });
}

QPushButton* SignalPlotPanel::filterButton() const { return m_filterBtn; }
bool SignalPlotPanel::isPaused() const { return m_pauseBtn->isChecked(); }

void SignalPlotPanel::setData(const QVector<double> &x, const QVector<double> &y)
{
    if (x.isEmpty() || y.isEmpty()) return;

    m_x = x;
    m_y = y;
    applyInversion();

    // 仅在首次初始化 X 轴范围，避免数据刚开始时出现毫秒级刻度
    // 之后用户可自由拖动、缩放，不会被打回
    if (!m_rangeInitialized) {
        const double rangeSpan = 20.0;
        m_plot->xAxis->setRange(0, rangeSpan);
        m_plot->yAxis->rescale(true);
        m_rangeInitialized = true;
    }

    m_plot->replot();
}

void SignalPlotPanel::setSnapshot(const QVector<double>& x, const QVector<double>& rawY, const QVector<double>& filtY)
{
    m_snapshotX = x;
    m_snapshotRawY = rawY;
    m_snapshotFiltY = filtY;
    m_hasSnapshot = true;
}

void SignalPlotPanel::clearSnapshot()
{
    m_snapshotX.clear();
    m_snapshotRawY.clear();
    m_snapshotFiltY.clear();
    m_hasSnapshot = false;
}

bool SignalPlotPanel::hasSnapshot() const { return m_hasSnapshot; }
QVector<double> SignalPlotPanel::snapshotX() const { return m_snapshotX; }
QVector<double> SignalPlotPanel::snapshotRawY() const { return m_snapshotRawY; }
QVector<double> SignalPlotPanel::snapshotFiltY() const { return m_snapshotFiltY; }

void SignalPlotPanel::applyInversion()
{
    if (m_x.isEmpty() || m_y.isEmpty()) return;
    QVector<double> displayY = m_y;
    if (m_inverted) {
        for (double &v : displayY) v = -v;
    }
    m_plot->graph(0)->setData(m_x, displayY);
}

//==========================================================
// MonitorTab 构造函数与析构
//==========================================================
MonitorTab::MonitorTab(QWidget *parent)
    : QWidget(parent),
    m_collectPoints(1000),
    m_averagePoints(5),
    m_showFiltA(false),
    m_showFiltB(false),
    m_showFiltAB(false),
    m_panelA(nullptr),
    m_panelB(nullptr),
    m_panelAB(nullptr),
    m_filterBtnA(nullptr),
    m_filterBtnB(nullptr),
    m_filterBtnAB(nullptr),
    m_comm(nullptr),
    m_feedbackTimer(new QTimer(this)),
    m_hasPendingTemp(false),
    m_hasPendingPowerA(false),
    m_hasPendingPowerB(false),
    m_hasPendingPrecision(false),
    m_hasPendingLevelA(false),
    m_hasPendingLevelB(false),
    m_hasPendingLevelAB(false),
    m_hasPendingZS(false),
    m_isSaving(false),
    m_startTime(QDateTime::currentMSecsSinceEpoch()),
    m_plotTimer(new QTimer(this))
{
    auto *mainLayout = new QHBoxLayout(this);

    // 左侧控制面板
    auto *leftGroup = new QGroupBox("TCD控制面板", this);
    leftGroup->setMaximumWidth(350);
    auto *leftLayout = new QVBoxLayout(leftGroup);
    setupControlPanel(leftLayout);
    mainLayout->addWidget(leftGroup, 1);

    // 右侧信号显示
    auto *rightGroup = new QGroupBox("信号显示", this);
    auto *rightLayout = new QVBoxLayout(rightGroup);
    setupSignalPanel(rightLayout);
    mainLayout->addWidget(rightGroup, 3);

    // 设置反馈定时器
    m_feedbackTimer->setInterval(2000);
    connect(m_feedbackTimer, &QTimer::timeout, this, &MonitorTab::checkSettingFeedback);
    m_feedbackTimer->start();

    // 灯丝安全检查定时器
    m_powerCheckTimer = new QTimer(this);
    m_powerCheckTimer->setSingleShot(true);
    connect(m_powerCheckTimer, &QTimer::timeout, this, &MonitorTab::onPowerCheckTimeout);

    // 绘图刷新定时器
    m_plotTimer->setInterval(50);
    connect(m_plotTimer, &QTimer::timeout, this, [this]() { updatePlots(false); });
    m_plotTimer->start();

    // 自动停止保存定时器
    m_autoStopTimer = new QTimer(this);
    m_autoStopTimer->setSingleShot(true);
    connect(m_autoStopTimer, &QTimer::timeout, this, &MonitorTab::stopDataSave);
}

MonitorTab::~MonitorTab()
{
    if (m_autoStopTimer && m_autoStopTimer->isActive())
        m_autoStopTimer->stop();
    if (m_saveFile.isOpen()) {
        m_saveStream.flush();
        m_saveFile.close();
    }
}

void MonitorTab::setCommunication(Communication *comm)
{
    m_comm = comm;
    if (m_comm) {
        connect(m_comm, &Communication::fastDataUpdated, this, &MonitorTab::onDataUpdated);
    }
}

void MonitorTab::onDataUpdated()
{
    if (!m_comm) return;

    // 只在 TCD 电源开启时记录数据
    if (!m_tcdPowered) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double timeSec = (m_tcdActiveMs + (now - m_tcdLastTick)) / 1000.0;

    double rawA = m_comm->tcdVoltageA();
    double rawB = m_comm->tcdVoltageB();
    double rawAB = m_comm->tcdVoltageAB();

    m_time.append(timeSec);
    m_rawA.append(rawA);
    m_rawB.append(rawB);
    m_rawAB.append(rawAB);

    while (m_time.size() > m_collectPoints) {
        m_time.removeFirst();
        m_rawA.removeFirst();
        m_rawB.removeFirst();
        m_rawAB.removeFirst();
        if (!m_filtA.isEmpty()) m_filtA.removeFirst();
        if (!m_filtB.isEmpty()) m_filtB.removeFirst();
        if (!m_filtAB.isEmpty()) m_filtAB.removeFirst();
    }

    // 因果滤波（支持 NaN，断开处保持断开）
    int window = qMax(1, m_averagePoints);
    auto smooth = [window](const QVector<double> &input) -> QVector<double> {
        QVector<double> output(input.size());
        for (int i = 0; i < input.size(); ++i) {
            if (qIsNaN(input[i])) {
                output[i] = qQNaN();
                continue;
            }
            int start = qMax(0, i - window + 1);
            int end = i;
            double sum = 0.0;
            int count = 0;
            for (int j = start; j <= end; ++j) {
                if (qIsNaN(input[j])) continue;
                sum += input[j];
                count++;
            }
            output[i] = (count > 0) ? (sum / count) : qQNaN();
        }
        return output;
    };
    m_filtA = smooth(m_rawA);
    m_filtB = smooth(m_rawB);
    m_filtAB = smooth(m_rawAB);

    updatePlots(false);

    // 文件写入（若保存中）
    if (m_isSaving && m_saveFile.isOpen() && !m_time.isEmpty()) {
        int idx = m_time.size() - 1;

        QString dataLine;
        if (qIsNaN(m_rawA.at(idx))) {
            // 断开点：写入 nan
            dataLine = QString("%1 nan nan nan nan nan nan")
                           .arg(m_time.at(idx), 0, 'f', 3);
        } else {
            dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(m_time.at(idx), 0, 'f', 3)
                .arg(m_rawA.at(idx), 0, 'f', 4)
                .arg(m_filtA.at(idx), 0, 'f', 4)
                .arg(m_rawB.at(idx), 0, 'f', 4)
                .arg(m_filtB.at(idx), 0, 'f', 4)
                .arg(m_rawAB.at(idx), 0, 'f', 4)
                .arg(m_filtAB.at(idx), 0, 'f', 4);
        }
        m_saveStream << dataLine << "\n";
    }
}

//==========================================================
// 控制面板
//==========================================================
void MonitorTab::setupControlPanel(QVBoxLayout *layout)
{
    QWidget *parent = layout->parentWidget();

    // ---------- 基本控制 ----------
    auto *basicGroup = new QGroupBox("基本控制", parent);
    auto *basicLayout = new QVBoxLayout(basicGroup);

    QPushButton *refreshBtn = new QPushButton("刷新", basicGroup);
    connect(refreshBtn, &QPushButton::clicked, this, &MonitorTab::refreshParameters);
    basicLayout->addWidget(refreshBtn);

    // TCD 温度
    auto *row1 = new QHBoxLayout;
    row1->addWidget(new QLabel("TCD温度:", basicGroup));
    m_tempEdit = new QSpinBox(basicGroup);
    m_tempEdit->setRange(0, 500);
    m_tempEdit->setSuffix(" ℃");
    QPushButton *setTempBtn = new QPushButton("设置", basicGroup);
    setTempBtn->setToolTip("TS 98\r  设置TCD温度");
    row1->addWidget(m_tempEdit);
    row1->addWidget(setTempBtn);
    basicLayout->addLayout(row1);
    connect(setTempBtn, &QPushButton::clicked, this, &MonitorTab::setTemperature);

    // ZS 启动调节流程
    auto *rowZS = new QHBoxLayout;
    rowZS->addWidget(new QLabel("启动调节:", basicGroup));
    m_zsEdit = new QSpinBox(basicGroup);
    m_zsEdit->setRange(0, 4);
    QPushButton *setZSBtn = new QPushButton("设置", basicGroup);
    setZSBtn->setToolTip("1：ZS \r 2：ZR \r  3：ZB \r 4：ZA \r");
    rowZS->addWidget(m_zsEdit);
    rowZS->addWidget(setZSBtn);
    basicLayout->addLayout(rowZS);
    connect(setZSBtn, &QPushButton::clicked, this, &MonitorTab::setZS);

    // 最小精度
    auto *rowPrec = new QHBoxLayout;
    rowPrec->addWidget(new QLabel("最小精度:", basicGroup));
    m_precisionEdit = new QComboBox(basicGroup);
    m_precisionEdit->addItem("0.001", 1);
    m_precisionEdit->addItem("0.01", 10);
    m_precisionEdit->addItem("0.05", 50);
    m_precisionEdit->addItem("0.1", 100);
    m_precisionEdit->addItem("1", 1000);
    QPushButton *setPrecBtn = new QPushButton("设置", basicGroup);
    setPrecBtn->setToolTip("PF 0.05\r   设置最小精度[]");
    rowPrec->addWidget(m_precisionEdit);
    rowPrec->addWidget(setPrecBtn);
    basicLayout->addLayout(rowPrec);
    connect(setPrecBtn, &QPushButton::clicked, this, &MonitorTab::setPrecision);

    // RF 复位
    auto *rowRF = new QHBoxLayout;
    m_rfBtn = new QPushButton("复位调节流程错误标志", basicGroup);
    m_rfBtn->setToolTip("收到01数据设备重置");
    rowRF->addWidget(m_rfBtn);
    basicLayout->addLayout(rowRF);
    connect(m_rfBtn, &QPushButton::clicked, this, &MonitorTab::setRF);

    layout->addWidget(basicGroup);

    // ---------- 灯丝功率 ----------
    auto *powerGroup = new QGroupBox("FA/FB功率", parent);
    auto *powerLayout = new QVBoxLayout(powerGroup);


    auto *powerRowA = new QHBoxLayout;
    powerRowA->addWidget(new QLabel("FA功率:", powerGroup));
    m_powerAEdit = new QSpinBox(powerGroup);
    m_powerAEdit->setRange(0, 100);
    m_powerAEdit->setSuffix(" %");
    QPushButton *setPowerABtn = new QPushButton("设置", powerGroup);
    setPowerABtn->setToolTip("FA 63\r 设置灯丝A功率");
    powerRowA->addWidget(m_powerAEdit);
    powerRowA->addWidget(setPowerABtn);
    powerLayout->addLayout(powerRowA);

    auto *powerRowB = new QHBoxLayout;
    powerRowB->addWidget(new QLabel("FB功率:", powerGroup));
    m_powerBEdit = new QSpinBox(powerGroup);
    m_powerBEdit->setRange(0, 100);
    m_powerBEdit->setSuffix(" %");
    QPushButton *setPowerBBtn = new QPushButton("设置", powerGroup);
    setPowerBBtn->setToolTip("FB 63\r  设置灯丝B功率");
    powerRowB->addWidget(m_powerBEdit);
    powerRowB->addWidget(setPowerBBtn);
    powerLayout->addLayout(powerRowB);

    powerRowB->addWidget(m_powerBEdit);
    powerRowB->addWidget(setPowerBBtn);
    powerLayout->addLayout(powerRowB);

    layout->addWidget(powerGroup);
    connect(setPowerABtn, &QPushButton::clicked, this, &MonitorTab::setPowerA);
    connect(setPowerBBtn, &QPushButton::clicked, this, &MonitorTab::setPowerB);

    // ---------- 电平设置（QDoubleSpinBox，范围 -1.2~1.2）----------
    auto *levelGroup = new QGroupBox("电平设置", parent);
    auto *levelLayout = new QVBoxLayout(levelGroup);

    auto *levelRowA = new QHBoxLayout;
    levelRowA->addWidget(new QLabel("A电平:", levelGroup));
    m_levelAEdit = new QDoubleSpinBox(levelGroup);
    m_levelAEdit->setRange(-1.2, 1.2);
    m_levelAEdit->setDecimals(1);
    m_levelAEdit->setSingleStep(0.1);
    m_levelAEdit->setSuffix(" V");
    QPushButton *setLevelABtn = new QPushButton("设置", levelGroup);
    setLevelABtn->setToolTip("ZA 0.1\r");
    levelRowA->addWidget(m_levelAEdit);
    levelRowA->addWidget(setLevelABtn);
    levelLayout->addLayout(levelRowA);

    auto *levelRowB = new QHBoxLayout;
    levelRowB->addWidget(new QLabel("B电平:", levelGroup));
    m_levelBEdit = new QDoubleSpinBox(levelGroup);
    m_levelBEdit->setRange(-1.2, 1.2);
    m_levelBEdit->setDecimals(1);
    m_levelBEdit->setSingleStep(0.1);
    m_levelBEdit->setSuffix(" V");
    QPushButton *setLevelBBtn = new QPushButton("设置", levelGroup);
    setLevelBBtn->setToolTip("ZB 0.1\r");
    levelRowB->addWidget(m_levelBEdit);
    levelRowB->addWidget(setLevelBBtn);
    levelLayout->addLayout(levelRowB);

    auto *levelRowAB = new QHBoxLayout;
    levelRowAB->addWidget(new QLabel("AB电平:", levelGroup));
    m_levelABEdit = new QDoubleSpinBox(levelGroup);
    m_levelABEdit->setRange(-1.2, 1.2);
    m_levelABEdit->setDecimals(1);
    m_levelABEdit->setSingleStep(0.1);
    m_levelABEdit->setSuffix(" V");
    QPushButton *setLevelABBtn = new QPushButton("设置", levelGroup);
    setLevelABBtn->setToolTip("ZR 0.1\r");
    levelRowAB->addWidget(m_levelABEdit);
    levelRowAB->addWidget(setLevelABBtn);
    levelLayout->addLayout(levelRowAB);

    layout->addWidget(levelGroup);
    connect(setLevelABtn, &QPushButton::clicked, this, &MonitorTab::setLevelA);
    connect(setLevelBBtn, &QPushButton::clicked, this, &MonitorTab::setLevelB);
    connect(setLevelABBtn, &QPushButton::clicked, this, &MonitorTab::setLevelAB);

    // ---------- 文件保存 ----------
    auto *fileGroup = new QGroupBox("文件保存", parent);
    auto *fileLayout = new QVBoxLayout(fileGroup);

    auto *fileNameRow = new QHBoxLayout;
    fileNameRow->addWidget(new QLabel("文件名:", fileGroup));
    m_fileNameEdit = new QLineEdit("data", fileGroup);
    fileNameRow->addWidget(m_fileNameEdit);
    fileLayout->addLayout(fileNameRow);

    auto *durationRow = new QHBoxLayout;
    durationRow->addWidget(new QLabel("保存时长:", fileGroup));
    m_durationSpin = new QSpinBox(fileGroup);
    m_durationSpin->setRange(1, 1440);
    m_durationSpin->setSuffix(" min");

    QSettings settings("MyCompany", "MicroGC");
    int savedDuration = settings.value("monitor/durationMinutes", 1).toInt();
    m_durationSpin->setValue(savedDuration);
    connect(m_durationSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int val) {
        QSettings s("MyCompany", "MicroGC");
        s.setValue("monitor/durationMinutes", val);
        s.sync();
    });

    durationRow->addWidget(m_durationSpin);
    durationRow->addStretch();
    fileLayout->addLayout(durationRow);

    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(new QLabel("路径:", fileGroup));
    m_pathEdit = new QLineEdit("./data", fileGroup);
    QPushButton *choosePathBtn = new QPushButton("选择路径", fileGroup);
    pathRow->addWidget(m_pathEdit);
    pathRow->addWidget(choosePathBtn);
    fileLayout->addLayout(pathRow);

    QPushButton *stopBtn = new QPushButton("停止", fileGroup);
    fileLayout->addWidget(stopBtn);

    layout->addWidget(fileGroup);
    connect(choosePathBtn, &QPushButton::clicked, this, &MonitorTab::choosePath);
    connect(stopBtn, &QPushButton::clicked, this, &MonitorTab::stopFileSave);
}

void MonitorTab::setupSignalPanel(QVBoxLayout *layout)
{
    QWidget *parent = layout->parentWidget();

    m_panelA = new SignalPlotPanel("A", parent);
    m_panelB = new SignalPlotPanel("B", parent);
    m_panelAB = new SignalPlotPanel("A-B", parent);

    m_filterBtnA = m_panelA->filterButton();
    m_filterBtnB = m_panelB->filterButton();
    m_filterBtnAB = m_panelAB->filterButton();

    connect(m_filterBtnA, &QPushButton::clicked, this, &MonitorTab::toggleFilterA);
    connect(m_filterBtnB, &QPushButton::clicked, this, &MonitorTab::toggleFilterB);
    connect(m_filterBtnAB, &QPushButton::clicked, this, &MonitorTab::toggleFilterAB);

    connect(m_panelA, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_rawA.size(), m_filtA.size()));
            m_panelA->setSnapshot(m_time.mid(0, len), m_rawA.mid(0, len), m_filtA.mid(0, len));
        } else {
            m_panelA->clearSnapshot();
        }
    });
    connect(m_panelB, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_rawB.size(), m_filtB.size()));
            m_panelB->setSnapshot(m_time.mid(0, len), m_rawB.mid(0, len), m_filtB.mid(0, len));
        } else {
            m_panelB->clearSnapshot();
        }
    });
    connect(m_panelAB, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_rawAB.size(), m_filtAB.size()));
            m_panelAB->setSnapshot(m_time.mid(0, len), m_rawAB.mid(0, len), m_filtAB.mid(0, len));
        } else {
            m_panelAB->clearSnapshot();
        }
    });

    layout->addWidget(m_panelA, 1);
    layout->addWidget(m_panelB, 1);
    layout->addWidget(m_panelAB, 1);
}

void MonitorTab::setTcdPowered(bool powered)
{
    if (m_tcdPowered == powered) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (powered) {
        // 电源开启，记录本次活动段起点
        m_tcdLastTick = now;
    } else {
        // 电源关闭，累积活动时间
        if (m_tcdLastTick > 0) {
            m_tcdActiveMs += now - m_tcdLastTick;
            m_tcdLastTick = 0;
        }

        // 插入 NaN 点，断开曲线
        double lastTimeSec = m_tcdActiveMs / 1000.0;
        m_time.append(lastTimeSec);
        m_rawA.append(qQNaN());
        m_rawB.append(qQNaN());
        m_rawAB.append(qQNaN());
        m_filtA.append(qQNaN());
        m_filtB.append(qQNaN());
        m_filtAB.append(qQNaN());

        // 立即写入文件（不等下次 onDataUpdated）
        if (m_isSaving && m_saveFile.isOpen()) {
            QString dataLine = QString("%1 nan nan nan nan nan nan")
            .arg(lastTimeSec, 0, 'f', 3);
            m_saveStream << dataLine << "\n";
            m_saveStream.flush();
        }

        // 立即刷新图像，显示断开
        updatePlots(true);
    }

    m_tcdPowered = powered;
}


//==========================================================
// 参数读写
//==========================================================
int MonitorTab::getParameter(const QString &key) const
{
    if (key == "temperature")     return m_tempEdit->value();
    if (key == "powerA")          return m_powerAEdit->value();
    if (key == "powerB")          return m_powerBEdit->value();
    if (key == "levelA")          return qRound(m_levelAEdit->value() * 10.0);
    if (key == "levelB")          return qRound(m_levelBEdit->value() * 10.0);
    if (key == "levelAB")         return qRound(m_levelABEdit->value() * 10.0);
    if (key == "zs")              return m_zsEdit->value();
    if (key == "precision")       return m_precisionEdit->currentData().toInt();
    if (key == "collectPoints")   return m_collectPoints;
    if (key == "averagePoints")   return m_averagePoints;
    return 0;
}

void MonitorTab::setParameter(const QString &key, int value)
{
    if (key == "temperature") {
        m_tempEdit->setValue(value);
    } else if (key == "powerA") {
        m_powerAEdit->setValue(value);
    } else if (key == "powerB") {
        m_powerBEdit->setValue(value);
    } else if (key == "levelA") {
        m_levelAEdit->setValue(value / 10.0);
    } else if (key == "levelB") {
        m_levelBEdit->setValue(value / 10.0);
    } else if (key == "levelAB") {
        m_levelABEdit->setValue(value / 10.0);
    } else if (key == "zs") {
        m_zsEdit->setValue(value);
    } else if (key == "precision") {
        int idx = m_precisionEdit->findData(value);
        if (idx >= 0) m_precisionEdit->setCurrentIndex(idx);
    } else if (key == "collectPoints") {
        m_collectPoints = value;
        updateDisplayLength();
    } else if (key == "averagePoints") {
        m_averagePoints = value;
    }
}

//==========================================================
// 设置命令
//==========================================================
void MonitorTab::setTemperature()
{
    if (!m_comm) return;
    int val = m_tempEdit->value();
    m_comm->setTcdTemperature(static_cast<quint16>(val));
    m_pendingTemp = val;
    m_hasPendingTemp = true;
    emit logMessage("TCD", QString("TCD温度设置已发送: %1 ℃").arg(val));
}

void MonitorTab::setPowerA()
{
    if (!m_comm) return;
    int val = m_powerAEdit->value();

    // TODO: 设置灯丝功率前检查TCD是否上电，暂时不确定使用
    // checkTcdPoweredBeforeSetPower(true, static_cast<quint16>(val));

    // 直接发送 FA 功率设置
    m_comm->setLampPowerA(static_cast<quint16>(val));
    m_pendingPowerA = val;
    m_hasPendingPowerA = true;
    emit logMessage("TCD", QString("FA功率设置已发送: %1 %").arg(val));
}

void MonitorTab::setPowerB()
{
    if (!m_comm) return;
    int val = m_powerBEdit->value();

    // TODO: 设置灯丝功率前检查TCD是否上电，暂时不确定使用
    // checkTcdPoweredBeforeSetPower(false, static_cast<quint16>(val));

    // 直接发送 FB 功率设置
    m_comm->setLampPowerB(static_cast<quint16>(val));
    m_pendingPowerB = val;
    m_hasPendingPowerB = true;
    emit logMessage("TCD", QString("FB功率设置已发送: %1 %").arg(val));
}

void MonitorTab::setLevelA()
{
    if (!m_comm) return;
    double val = m_levelAEdit->value();                        // -1.2 ~ 1.2
    qint16 raw = static_cast<qint16>(qRound(val * 10.0));      // -12 ~ 12
    m_comm->setChannelAVoltage(static_cast<quint16>(raw));
    m_pendingLevelA = static_cast<quint16>(raw);
    m_hasPendingLevelA = true;
    emit logMessage("TCD", QString("A电平设置已发送: %1 V (raw=%2)").arg(val, 0, 'f', 1).arg(raw));
}

void MonitorTab::setLevelB()
{
    if (!m_comm) return;
    double val = m_levelBEdit->value();
    qint16 raw = static_cast<qint16>(qRound(val * 10.0));
    m_comm->setChannelBVoltage(static_cast<quint16>(raw));
    m_pendingLevelB = static_cast<quint16>(raw);
    m_hasPendingLevelB = true;
    emit logMessage("TCD", QString("B电平设置已发送: %1 V (raw=%2)").arg(val, 0, 'f', 1).arg(raw));
}

void MonitorTab::setLevelAB()
{
    if (!m_comm) return;
    double val = m_levelABEdit->value();
    qint16 raw = static_cast<qint16>(qRound(val * 10.0));
    m_comm->setChannelABVoltage(static_cast<quint16>(raw));
    m_pendingLevelAB = static_cast<quint16>(raw);
    m_hasPendingLevelAB = true;
    emit logMessage("TCD", QString("AB电平设置已发送: %1 V (raw=%2)").arg(val, 0, 'f', 1).arg(raw));
}

void MonitorTab::setZS()
{
    if (!m_comm) return;
    int val = m_zsEdit->value();
    if (val < 0 || val > 4) return;
    m_comm->setZS(static_cast<quint16>(val));
    m_pendingZS = val;
    m_hasPendingZS = true;
    emit logMessage("TCD", QString("启动调节流程已发送: %1").arg(val));
}

void MonitorTab::setPrecision()
{
    if (!m_comm) return;
    int val = m_precisionEdit->currentData().toInt();
    m_comm->setPrecision(static_cast<quint16>(val));
    m_pendingPrecision = val;
    m_hasPendingPrecision = true;
    emit logMessage("TCD", QString("最小精度设置已发送: %1").arg(val));
}

void MonitorTab::setRF()
{
    if (!m_comm) return;
    m_comm->setRF(1);
    emit logMessage("TCD", "复位调节流程错误标志已发送");
}

void MonitorTab::applyGlobalParameters()
{
    if (!m_comm) return;

    // TCD 温度
    setTemperature();

    // ZA/ZB/ZR 电压
    setLevelA();
    setLevelB();
    setLevelAB();

    // 精度
    setPrecision();
}

//==========================================================
// TCD 通电检查（灯丝设置前）
//==========================================================
void MonitorTab::checkTcdPoweredBeforeSetPower(bool isPowerA, quint16 value)
{
    if (m_powerCheckInProgress) {
        emit logMessage("TCD", "正在检查TCD通电状态，请稍候...");
        return;
    }

    m_powerCheckInProgress = true;
    m_powerCheckIsA = isPowerA;

    // 直接读 0x000A（最新一次慢速轮询缓存，或主动发起一次）
    // 为保证实时性，这里主动发一次读取请求
    m_comm->requestRegisterRead(0x000A, [this, isPowerA, value](quint16 diag) {
        m_powerCheckInProgress = false;

        bool tc4Fail = (diag & (1 << 0)) != 0;
        bool tcdFail = (diag & (1 << 1)) != 0;
        bool powered = !(tc4Fail || tcdFail);

        if (powered) {
            emit logMessage("TCD", "TCD已通电，禁止设置灯丝功率！");
            return;
        }

        // 未通电，允许设置灯丝
        if (isPowerA) {
            m_comm->setLampPowerA(value);
            m_pendingPowerA = value;
            m_hasPendingPowerA = true;
            emit logMessage("TCD", QString("灯丝功率A设置已发送: %1 %").arg(value));
        } else {
            m_comm->setLampPowerB(value);
            m_pendingPowerB = value;
            m_hasPendingPowerB = true;
            emit logMessage("TCD", QString("灯丝功率B设置已发送: %1 %").arg(value));
        }
    });
}

void MonitorTab::onPowerCheckTimeout()
{
    m_powerCheckAttempts++;

    if (m_powerCheckReplyReceived) {
        m_powerCheckInProgress = false;
        m_powerCheckTimer->stop();
        emit logMessage("TCD", "TCD已通电，禁止设置灯丝功率！");
        return;
    }

    if (m_powerCheckAttempts < 2) {
        m_powerCheckReplyReceived = false;
        m_comm->requestRegisterRead(0x0009, [this](quint16) {   // 改为 0x0009
            m_powerCheckReplyReceived = true;
        });
        m_powerCheckTimer->start(300);
    } else {
        m_powerCheckInProgress = false;
        m_powerCheckTimer->stop();

        quint16 val = (m_powerCheckIsA) ? m_powerAEdit->value() : m_powerBEdit->value();
        if (m_powerCheckIsA) {
            m_comm->setLampPowerA(val);
            m_pendingPowerA = val;
            m_hasPendingPowerA = true;
            emit logMessage("TCD", QString("灯丝功率A设置已发送: %1 %").arg(val));
        } else {
            m_comm->setLampPowerB(val);
            m_pendingPowerB = val;
            m_hasPendingPowerB = true;
            emit logMessage("TCD", QString("灯丝功率B设置已发送: %1 %").arg(val));
        }
    }
}

//==========================================================
// 设置反馈验证
//==========================================================
void MonitorTab::checkSettingFeedback()
{
    if (!m_comm) return;

    auto verify = [this](const QString &name, quint16 addr, quint16 expected,
                         bool &flag, int &retry, auto resend) {
        if (!flag) return;
        m_comm->requestRegisterRead(addr, [=, &flag, &retry](quint16 actual) {
            if (!flag) return;
            if (actual == expected) {
                flag = false;
                retry = 0;
                emit logMessage("TCD", QString("%1 设置成功").arg(name));
            } else {
                if (retry < 3) {
                    resend();
                    retry++;
                    emit logMessage("TCD", QString("%1 设置失败，重试(%2/3)").arg(name).arg(retry));
                } else {
                    flag = false;
                    retry = 0;
                    emit logMessage("TCD", QString("%1 设置失败，超过最大重试次数").arg(name));
                }
            }
        });
    };

    verify("TCD温度", 0x03E8, m_pendingTemp, m_hasPendingTemp, m_retryTemp,
           [this]() { m_comm->setTcdTemperature(m_pendingTemp); });

    verify("FA功率", 0x03E9, m_pendingPowerA, m_hasPendingPowerA, m_retryPowerA,
           [this]() { m_comm->setLampPowerA(m_pendingPowerA); });

    verify("FB功率", 0x03EA, m_pendingPowerB, m_hasPendingPowerB, m_retryPowerB,
           [this]() { m_comm->setLampPowerB(m_pendingPowerB); });

    verify("最小精度", 0x03F1, m_pendingPrecision, m_hasPendingPrecision, m_retryPrecision,
           [this]() { m_comm->setPrecision(m_pendingPrecision); });

    verify("A电平", 0x03ED, m_pendingLevelA, m_hasPendingLevelA, m_retryLevelA,
           [this]() { m_comm->setChannelAVoltage(m_pendingLevelA); });

    verify("B电平", 0x03EE, m_pendingLevelB, m_hasPendingLevelB, m_retryLevelB,
           [this]() { m_comm->setChannelBVoltage(m_pendingLevelB); });

    verify("AB电平", 0x03EF, m_pendingLevelAB, m_hasPendingLevelAB, m_retryLevelAB,
           [this]() { m_comm->setChannelABVoltage(m_pendingLevelAB); });
}

void MonitorTab::refreshParameters()
{
    if (!m_comm) return;

    // 0x03E8 TCD温度设置值
    m_comm->requestRegisterRead(0x03E8, [this](quint16 v) {
        if (m_tempEdit) m_tempEdit->setValue(v);
    });

    // 0x03E9 A灯丝功率
    m_comm->requestRegisterRead(0x03E9, [this](quint16 v) {
        if (m_powerAEdit) m_powerAEdit->setValue(v);
    });

    // 0x03EA B灯丝功率
    m_comm->requestRegisterRead(0x03EA, [this](quint16 v) {
        if (m_powerBEdit) m_powerBEdit->setValue(v);
    });

    // 0x03ED A电平（s16，读回后除以 10 得到电压）
    m_comm->requestRegisterRead(0x03ED, [this](quint16 v) {
        qint16 raw = static_cast<qint16>(v);
        if (m_levelAEdit) m_levelAEdit->setValue(raw / 10.0);
    });

    // 0x03EE B电平
    m_comm->requestRegisterRead(0x03EE, [this](quint16 v) {
        qint16 raw = static_cast<qint16>(v);
        if (m_levelBEdit) m_levelBEdit->setValue(raw / 10.0);
    });

    // 0x03EF AB电平
    m_comm->requestRegisterRead(0x03EF, [this](quint16 v) {
        qint16 raw = static_cast<qint16>(v);
        if (m_levelABEdit) m_levelABEdit->setValue(raw / 10.0);
    });

    // 0x03F0 ZS 启动调节流程
    m_comm->requestRegisterRead(0x03F0, [this](quint16 v) {
        if (m_zsEdit) m_zsEdit->setValue(v);
    });

    // 0x03F1 PF 最小精度
    m_comm->requestRegisterRead(0x03F1, [this](quint16 v) {
        if (m_precisionEdit) {
            int idx = m_precisionEdit->findData(static_cast<int>(v));
            if (idx >= 0) m_precisionEdit->setCurrentIndex(idx);
        }
    });

    // 0x03F2 RF 复位标志（命令型，仅日志记录）
    m_comm->requestRegisterRead(0x03F2, [this](quint16 v) {
        emit logMessage("TCD", QString("RF 复位标志当前值: %1").arg(v));
    });

    emit logMessage("TCD", "刷新参数：已请求读取所有设置值");
}

//==========================================================
// 文件保存
//==========================================================
void MonitorTab::startAutoSave()
{
    if (m_isSaving) return;

    QString fileName = m_fileNameEdit->text().trimmed();
    if (fileName.isEmpty()) fileName = "data";
    QString fullPath = m_pathEdit->text().trimmed();
    if (fullPath.isEmpty()) fullPath = "./data";
    QDir dir;
    dir.mkpath(fullPath);
    if (!fullPath.endsWith('/') && !fullPath.endsWith('\\')) fullPath += '/';
    fullPath += fileName + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt";

    startDataSave(fullPath, m_durationSpin->value());
    emit logMessage("文件操作", QString("开始保存数据至 %1").arg(fullPath));
}

void MonitorTab::startDataSave(const QString &fileName, int durationMinutes)
{
    m_isSaving = true;
    if (m_saveFile.isOpen()) {
        m_saveStream.flush();
        m_saveFile.close();
    }

    m_saveFile.setFileName(fileName);
    if (!m_saveFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "无法打开文件:" << fileName;
        m_isSaving = false;
        return;
    }
    m_saveStream.setDevice(&m_saveFile);
    m_saveStream << "时间 滤波前A 滤波后A 滤波前B 滤波后B 滤波前A-B 滤波后A-B\n";

    // 写入已缓冲的数据（含 NaN 断开点）
    for (int i = 0; i < m_time.size(); ++i) {
        QString dataLine;
        if (qIsNaN(m_rawA.at(i))) {
            dataLine = QString("%1 nan nan nan nan nan nan")
            .arg(m_time.at(i), 0, 'f', 3);
        } else {
            dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(m_time.at(i), 0, 'f', 3)
                .arg(m_rawA.at(i), 0, 'f', 4)
                .arg(m_filtA.at(i), 0, 'f', 4)
                .arg(m_rawB.at(i), 0, 'f', 4)
                .arg(m_filtB.at(i), 0, 'f', 4)
                .arg(m_rawAB.at(i), 0, 'f', 4)
                .arg(m_filtAB.at(i), 0, 'f', 4);
        }
        m_saveStream << dataLine << "\n";
    }

    if (durationMinutes > 0) {
        m_autoStopTimer->start(durationMinutes * 60 * 1000);
    }
}

void MonitorTab::stopDataSave()
{
    m_isSaving = false;
    m_autoStopTimer->stop();
    if (m_saveFile.isOpen()) {
        m_saveStream.flush();
        m_saveFile.close();
    }
}

void MonitorTab::choosePath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择保存路径", m_pathEdit->text());
    if (!dir.isEmpty())
        m_pathEdit->setText(dir);
    emit logMessage("文件操作", QString("保存路径设置为 %1").arg(m_pathEdit->text()));
}

void MonitorTab::stopFileSave()
{
    stopDataSave();
    emit logMessage("文件操作", "停止写入数据文件");
}

//==========================================================
// 绘图
//==========================================================
void MonitorTab::updateDisplayLength()
{
    while (m_time.size() > m_collectPoints) {
        m_time.removeFirst();
        m_rawA.removeFirst();
        m_rawB.removeFirst();
        m_rawAB.removeFirst();
        if (!m_filtA.isEmpty()) m_filtA.removeFirst();
        if (!m_filtB.isEmpty()) m_filtB.removeFirst();
        if (!m_filtAB.isEmpty()) m_filtAB.removeFirst();
    }
}

void MonitorTab::updatePlots(bool force)
{
    if (m_time.isEmpty()) return;

    if (m_panelA->isPaused()) {
        if (force && m_panelA->hasSnapshot()) {
            QVector<double> x = m_panelA->snapshotX();
            QVector<double> y = m_showFiltA ? m_panelA->snapshotFiltY() : m_panelA->snapshotRawY();
            if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
                m_panelA->setData(x, y);
        }
    } else {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltA ? m_filtA : m_rawA;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelA->setData(x, y);
    }

    if (m_panelB->isPaused()) {
        if (force && m_panelB->hasSnapshot()) {
            QVector<double> x = m_panelB->snapshotX();
            QVector<double> y = m_showFiltB ? m_panelB->snapshotFiltY() : m_panelB->snapshotRawY();
            if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
                m_panelB->setData(x, y);
        }
    } else {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltB ? m_filtB : m_rawB;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelB->setData(x, y);
    }

    if (m_panelAB->isPaused()) {
        if (force && m_panelAB->hasSnapshot()) {
            QVector<double> x = m_panelAB->snapshotX();
            QVector<double> y = m_showFiltAB ? m_panelAB->snapshotFiltY() : m_panelAB->snapshotRawY();
            if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
                m_panelAB->setData(x, y);
        }
    } else {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltAB ? m_filtAB : m_rawAB;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelAB->setData(x, y);
    }
}

void MonitorTab::toggleFilterA()   { m_showFiltA = !m_showFiltA;   m_filterBtnA->setText(m_showFiltA ? "显示滤波后" : "显示滤波前");   updatePlots(true); }
void MonitorTab::toggleFilterB()   { m_showFiltB = !m_showFiltB;   m_filterBtnB->setText(m_showFiltB ? "显示滤波后" : "显示滤波前");   updatePlots(true); }
void MonitorTab::toggleFilterAB()  { m_showFiltAB = !m_showFiltAB; m_filterBtnAB->setText(m_showFiltAB ? "显示滤波后" : "显示滤波前"); updatePlots(true); }

void MonitorTab::setChannelAVisible(bool visible) { if (m_panelA) m_panelA->setVisible(visible); }
void MonitorTab::setChannelBVisible(bool visible) { if (m_panelB) m_panelB->setVisible(visible); }
void MonitorTab::setChannelABVisible(bool visible) { if (m_panelAB) m_panelAB->setVisible(visible); }

void MonitorTab::setCollectPoints(int points) { m_collectPoints = points; updateDisplayLength(); }
void MonitorTab::setAveragePoints(int points) { m_averagePoints = points; }