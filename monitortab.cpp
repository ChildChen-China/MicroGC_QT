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

        if (!m_x.isEmpty()) {
            double lastX = m_x.last();
            double firstX = qMax(0.0, lastX - 20.0);
            m_plot->xAxis->setRange(firstX, lastX);
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

    if (!m_rangeInitialized) {
        double lastX = x.last();
        double firstX = qMax(0.0, lastX - 20.0);
        m_plot->xAxis->setRange(firstX, lastX);
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

    // 设置反馈定时器（间隔2秒）
    m_feedbackTimer->setInterval(2000);
    connect(m_feedbackTimer, &QTimer::timeout, this, &MonitorTab::checkSettingFeedback);
    m_feedbackTimer->start();

    // 创建灯丝功率安全检查定时器（单次，300ms超时）
    m_powerCheckTimer = new QTimer(this);
    m_powerCheckTimer->setSingleShot(true);
    connect(m_powerCheckTimer, &QTimer::timeout, this, &MonitorTab::onPowerCheckTimeout);

    // 绘图刷新定时器（50ms）
    m_plotTimer->setInterval(50);
    connect(m_plotTimer, &QTimer::timeout, this, [this]() { updatePlots(false); });
    m_plotTimer->start();

    // 创建自动停止保存定时器（单次触发）
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

//==========================================================
// 通信设置与数据更新
//==========================================================
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

    double timeSec = (QDateTime::currentMSecsSinceEpoch() - m_startTime) / 1000.0;

    double rawA = m_comm->tcdVoltageA();
    double rawB = m_comm->tcdVoltageB();
    double rawAB = m_comm->tcdVoltageAB();

    // 追加原始数据
    m_time.append(timeSec);
    m_rawA.append(rawA);
    m_rawB.append(rawB);
    m_rawAB.append(rawAB);

    // 限制缓冲区长度
    while (m_time.size() > m_collectPoints) {
        m_time.removeFirst();
        m_rawA.removeFirst();
        m_rawB.removeFirst();
        m_rawAB.removeFirst();
        if (!m_filtA.isEmpty()) m_filtA.removeFirst();
        if (!m_filtB.isEmpty()) m_filtB.removeFirst();
        if (!m_filtAB.isEmpty()) m_filtAB.removeFirst();
    }

    // 因果滤波（仅使用过去和当前数据）
    int window = qMax(1, m_averagePoints);
    auto smooth = [window](const QVector<double> &input) -> QVector<double> {
        QVector<double> output(input.size());
        for (int i = 0; i < input.size(); ++i) {
            int start = qMax(0, i - window + 1);
            int end = i;
            double sum = 0.0;
            for (int j = start; j <= end; ++j) sum += input[j];
            output[i] = sum / (end - start + 1);
        }
        return output;
    };
    m_filtA = smooth(m_rawA);
    m_filtB = smooth(m_rawB);
    m_filtAB = smooth(m_rawAB);

    // 更新界面
    updatePlots(false);

    // 写入文件（若保存中）
    if (m_isSaving && m_saveFile.isOpen() && !m_time.isEmpty()) {
        int idx = m_time.size() - 1;
        QString dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
                               .arg(m_time.at(idx), 0, 'f', 3)
                               .arg(m_rawA.at(idx), 0, 'f', 4)
                               .arg(m_filtA.at(idx), 0, 'f', 4)
                               .arg(m_rawB.at(idx), 0, 'f', 4)
                               .arg(m_filtB.at(idx), 0, 'f', 4)
                               .arg(m_rawAB.at(idx), 0, 'f', 4)
                               .arg(m_filtAB.at(idx), 0, 'f', 4);
        m_saveStream << dataLine << "\n";
    }
}

//==========================================================
// UI 构建
//==========================================================
void MonitorTab::setupControlPanel(QVBoxLayout *layout)
{
    QWidget *parent = layout->parentWidget();

    // --- 基本控制 ---
    auto *basicGroup = new QGroupBox("基本控制", parent);
    auto *basicLayout = new QVBoxLayout(basicGroup);

    m_enableCheck = new QPushButton("开启检测器", basicGroup);
    m_enableCheck->setCheckable(true);
    m_enableCheck->setStyleSheet("background-color: gray; color: white;");
    connect(m_enableCheck, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_comm) return;
        m_comm->enableDetector(checked);
        m_enableCheck->setText(checked ? "关闭检测器" : "开启检测器");
        m_enableCheck->setStyleSheet(checked ? "background-color: red; color: white;" :
                                         "background-color: gray; color: white;");
        emit logMessage("TCD", checked ? "开启检测器加热与灯丝供电" : "关闭检测器加热与灯丝供电");
    });

    QPushButton *refreshBtn = new QPushButton("刷新", basicGroup);
    connect(refreshBtn, &QPushButton::clicked, this, &MonitorTab::refreshParameters);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(m_enableCheck);
    row1->addWidget(refreshBtn);
    row1->addStretch();
    basicLayout->addLayout(row1);

    auto *row3 = new QHBoxLayout;
    row3->addWidget(new QLabel("TCD温度设置:", basicGroup));
    m_tempEdit = new QSpinBox(basicGroup);
    m_tempEdit->setRange(-100, 500);
    m_tempEdit->setSuffix(" ℃");
    QPushButton *setTempBtn = new QPushButton("设置", basicGroup);
    row3->addWidget(m_tempEdit);
    row3->addWidget(setTempBtn);
    basicLayout->addLayout(row3);

    layout->addWidget(basicGroup);
    connect(setTempBtn, &QPushButton::clicked, this, &MonitorTab::setTemperature);

    // --- 灯丝功率 ---
    auto *powerGroup = new QGroupBox("灯丝功率", parent);
    auto *powerLayout = new QVBoxLayout(powerGroup);

    auto *powerRowA = new QHBoxLayout;
    powerRowA->addWidget(new QLabel("灯丝功率A:", powerGroup));
    m_powerAEdit = new QSpinBox(powerGroup);
    m_powerAEdit->setRange(0, 100);
    m_powerAEdit->setSuffix(" %");
    QPushButton *setPowerABtn = new QPushButton("设置", powerGroup);
    powerRowA->addWidget(m_powerAEdit);
    powerRowA->addWidget(setPowerABtn);
    powerLayout->addLayout(powerRowA);

    auto *powerRowB = new QHBoxLayout;
    powerRowB->addWidget(new QLabel("灯丝功率B:", powerGroup));
    m_powerBEdit = new QSpinBox(powerGroup);
    m_powerBEdit->setRange(0, 100);
    m_powerBEdit->setSuffix(" %");
    QPushButton *setPowerBBtn = new QPushButton("设置", powerGroup);
    powerRowB->addWidget(m_powerBEdit);
    powerRowB->addWidget(setPowerBBtn);
    powerLayout->addLayout(powerRowB);

    layout->addWidget(powerGroup);
    connect(setPowerABtn, &QPushButton::clicked, this, &MonitorTab::setPowerA);
    connect(setPowerBBtn, &QPushButton::clicked, this, &MonitorTab::setPowerB);

    // --- 电平设置 ---
    auto *levelGroup = new QGroupBox("电平设置", parent);
    auto *levelLayout = new QVBoxLayout(levelGroup);

    auto *levelRowA = new QHBoxLayout;
    levelRowA->addWidget(new QLabel("A电平(mV):", levelGroup));
    m_levelAEdit = new QSpinBox(levelGroup);
    m_levelAEdit->setRange(-1000, 1000);
    m_levelAEdit->setSuffix(" mV");
    QPushButton *setLevelABtn = new QPushButton("设置", levelGroup);
    levelRowA->addWidget(m_levelAEdit);
    levelRowA->addWidget(setLevelABtn);
    levelLayout->addLayout(levelRowA);

    auto *levelRowB = new QHBoxLayout;
    levelRowB->addWidget(new QLabel("B电平(mV):", levelGroup));
    m_levelBEdit = new QSpinBox(levelGroup);
    m_levelBEdit->setRange(-1000, 1000);
    m_levelBEdit->setSuffix(" mV");
    QPushButton *setLevelBBtn = new QPushButton("设置", levelGroup);
    levelRowB->addWidget(m_levelBEdit);
    levelRowB->addWidget(setLevelBBtn);
    levelLayout->addLayout(levelRowB);

    auto *levelRowAB = new QHBoxLayout;
    levelRowAB->addWidget(new QLabel("AB电平(mV):", levelGroup));
    m_levelABEdit = new QSpinBox(levelGroup);
    m_levelABEdit->setRange(-1000, 1000);
    m_levelABEdit->setSuffix(" mV");
    QPushButton *setLevelABBtn = new QPushButton("设置", levelGroup);
    levelRowAB->addWidget(m_levelABEdit);
    levelRowAB->addWidget(setLevelABBtn);
    levelLayout->addLayout(levelRowAB);

    auto *precisionRow = new QHBoxLayout;
    precisionRow->addWidget(new QLabel("精度:", levelGroup));
    m_precisionEdit = new QComboBox(levelGroup);
    m_precisionEdit->addItem("0.01", 1);
    m_precisionEdit->addItem("0.1", 10);
    m_precisionEdit->addItem("1", 100);
    QPushButton *setPrecisionBtn = new QPushButton("设置", levelGroup);
    precisionRow->addWidget(m_precisionEdit);
    precisionRow->addWidget(setPrecisionBtn);
    levelLayout->addLayout(precisionRow);

    layout->addWidget(levelGroup);
    connect(setLevelABtn, &QPushButton::clicked, this, &MonitorTab::setLevelA);
    connect(setLevelBBtn, &QPushButton::clicked, this, &MonitorTab::setLevelB);
    connect(setLevelABBtn, &QPushButton::clicked, this, &MonitorTab::setLevelAB);
    connect(setPrecisionBtn, &QPushButton::clicked, this, &MonitorTab::setPrecision);

    // --- 文件保存 ---
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

    // 从 QSettings 读取上次保存的时长
    QSettings settings("MyCompany", "MicroGC");
    int savedDuration = settings.value("monitor/durationMinutes", 1).toInt();
    m_durationSpin->setValue(savedDuration);

    // 当用户修改时长时，保存到 QSettings
    connect(m_durationSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int val) {
        QSettings settings("MyCompany", "MicroGC");
        settings.setValue("monitor/durationMinutes", val);
        settings.sync();
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

    // 暂停时保存快照
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

//==========================================================
// 参数读写
//==========================================================
int MonitorTab::getParameter(const QString &key) const
{
    if (key == "temperature")     return m_tempEdit->value();
    if (key == "powerA")          return m_powerAEdit->value();
    if (key == "powerB")          return m_powerBEdit->value();
    if (key == "levelA")          return m_levelAEdit->value();
    if (key == "levelB")          return m_levelBEdit->value();
    if (key == "levelAB")         return m_levelABEdit->value();
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
        m_levelAEdit->setValue(value);
    } else if (key == "levelB") {
        m_levelBEdit->setValue(value);
    } else if (key == "levelAB") {
        m_levelABEdit->setValue(value);
    } else if (key == "precision") {
        int idx = m_precisionEdit->findData(value);
        if (idx >= 0) m_precisionEdit->setCurrentIndex(idx);
    } else if (key == "collectPoints") {
        m_collectPoints = value;
        updateDisplayLength();
    } else if (key == "averagePoints") {
        m_averagePoints = value;
        // 滤波参数改变，下次 onDataUpdated 会自动使用新值
    }
}

//==========================================================
// 设置命令发送与验证
//==========================================================
void MonitorTab::setTemperature() { if (m_comm) { int val = m_tempEdit->value(); m_comm->setTcdTemperature(val); m_pendingTemp = val; m_hasPendingTemp = true; emit logMessage("TCD", QString("TCD温度设置已发送: %1 ℃").arg(val)); } }

void MonitorTab::setPowerA()
{
    if (!m_comm) return;
    int val = m_powerAEdit->value();
    checkTcdPoweredBeforeSetPower(true, static_cast<quint16>(val));
}

void MonitorTab::setPowerB()
{
    if (!m_comm) return;
    int val = m_powerBEdit->value();
    checkTcdPoweredBeforeSetPower(false, static_cast<quint16>(val));
}


void MonitorTab::setLevelA()     { if (m_comm) { int val = m_levelAEdit->value(); m_comm->setChannelAVoltage(val); m_pendingLevelA = val; m_hasPendingLevelA = true; emit logMessage("TCD", QString("A电平设置已发送: %1 mV").arg(val)); } }
void MonitorTab::setLevelB()     { if (m_comm) { int val = m_levelBEdit->value(); m_comm->setChannelBVoltage(val); m_pendingLevelB = val; m_hasPendingLevelB = true; emit logMessage("TCD", QString("B电平设置已发送: %1 mV").arg(val)); } }
void MonitorTab::setLevelAB()    { if (m_comm) { int val = m_levelABEdit->value(); m_comm->setChannelABVoltage(val); m_pendingLevelAB = val; m_hasPendingLevelAB = true; emit logMessage("TCD", QString("AB电平设置已发送: %1 mV").arg(val)); } }
void MonitorTab::setPrecision()  { if (m_comm) { int val = m_precisionEdit->currentData().toInt(); m_comm->setPrecision(val); m_pendingPrecision = val; m_hasPendingPrecision = true; emit logMessage("TCD", QString("最小精度设置已发送: 寄存器值 %1").arg(val)); } }

void MonitorTab::checkTcdPoweredBeforeSetPower(bool isPowerA, quint16 value)
{
    if (m_powerCheckInProgress) {
        emit logMessage("TCD", "正在检查TCD通电状态，请稍候...");
        return;
    }

    m_powerCheckInProgress = true;
    m_powerCheckIsA = isPowerA;
    m_powerCheckAttempts = 0;
    m_powerCheckReplyReceived = false;

    // 第一次读取 TCD 温度设置寄存器（0x03E8）
    m_comm->requestRegisterRead(0x03E8, [this](quint16) {
        m_powerCheckReplyReceived = true;
    });

    // 启动超时定时器（300ms后检查）
    m_powerCheckTimer->start(300);
}

void MonitorTab::onPowerCheckTimeout()
{
    m_powerCheckAttempts++;

    if (m_powerCheckReplyReceived) {
        // 有回复，说明 TCD 已通电，禁止设置灯丝功率
        m_powerCheckInProgress = false;
        m_powerCheckTimer->stop();
        emit logMessage("TCD", "TCD已通电，禁止设置灯丝功率！");
        return;
    }

    if (m_powerCheckAttempts < 2) {
        // 第一次无回复，进行第二次读取
        m_powerCheckReplyReceived = false;
        m_comm->requestRegisterRead(0x03E8, [this](quint16) {
            m_powerCheckReplyReceived = true;
        });
        m_powerCheckTimer->start(300);
    } else {
        // 两次均无回复，允许设置功率
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

void MonitorTab::applyGlobalParameters()
{
    QTimer::singleShot(0,    this, [this]() { setTemperature(); });
    QTimer::singleShot(600,  this, [this]() { setLevelA(); });
    QTimer::singleShot(800,  this, [this]() { setLevelB(); });
    QTimer::singleShot(1000, this, [this]() { setLevelAB(); });
    QTimer::singleShot(1200, this, [this]() { setPrecision(); });
}

void MonitorTab::setDetectorEnabled(bool enabled)
{
    if (!m_enableCheck) return;
    m_enableCheck->blockSignals(true);
    m_enableCheck->setChecked(enabled);
    m_enableCheck->setText(enabled ? "关闭检测器" : "开启检测器");
    m_enableCheck->setStyleSheet(enabled ? "background-color: red; color: white;" : "background-color: gray; color: white;");
    m_enableCheck->blockSignals(false);
}

void MonitorTab::checkSettingFeedback()
{
    if (!m_comm) return;

    auto verify = [this](const QString &name, quint16 addr, quint16 expected, bool &flag, int &retry, auto resend) {
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

    verify("TCD温度", 0x03E8, m_pendingTemp, m_hasPendingTemp, m_retryTemp, [this]() { m_comm->setTcdTemperature(m_pendingTemp); });
    verify("灯丝功率A", 0x03E9, m_pendingPowerA, m_hasPendingPowerA, m_retryPowerA, [this]() { m_comm->setLampPowerA(m_pendingPowerA); });
    verify("灯丝功率B", 0x03EA, m_pendingPowerB, m_hasPendingPowerB, m_retryPowerB, [this]() { m_comm->setLampPowerB(m_pendingPowerB); });
    verify("最小精度", 0x03F1, m_pendingPrecision, m_hasPendingPrecision, m_retryPrecision, [this]() { m_comm->setPrecision(m_pendingPrecision); });
    verify("A电平", 0x03ED, m_pendingLevelA, m_hasPendingLevelA, m_retryLevelA, [this]() { m_comm->setChannelAVoltage(m_pendingLevelA); });
    verify("B电平", 0x03EE, m_pendingLevelB, m_hasPendingLevelB, m_retryLevelB, [this]() { m_comm->setChannelBVoltage(m_pendingLevelB); });
    verify("AB电平", 0x03EF, m_pendingLevelAB, m_hasPendingLevelAB, m_retryLevelAB, [this]() { m_comm->setChannelABVoltage(m_pendingLevelAB); });
}

void MonitorTab::refreshParameters()
{
    if (!m_comm) return;

    // 1. 读取 TCD 温度，并直接更新设置温度输入框（不再使用单独的显示标签）
    m_comm->requestRegisterRead(0x0007, [this](quint16 value) {
        if (m_tempEdit) {
            // 假设温度寄存器为有符号16位，按实际协议调整
            qint16 temp = static_cast<qint16>(value);
            m_tempEdit->setValue(temp);
        }
    });

    // 2. 读取灯丝功率 A
    m_comm->requestRegisterRead(0x03E9, [this](quint16 value) {
        if (m_powerAEdit)
            m_powerAEdit->setValue(value);
    });

    // 3. 读取灯丝功率 B
    m_comm->requestRegisterRead(0x03EA, [this](quint16 value) {
        if (m_powerBEdit)
            m_powerBEdit->setValue(value);
    });

    // 4. 读取 A 电平
    m_comm->requestRegisterRead(0x03ED, [this](quint16 value) {
        if (m_levelAEdit)
            m_levelAEdit->setValue(static_cast<qint16>(value));
    });

    // 5. 读取 B 电平
    m_comm->requestRegisterRead(0x03EE, [this](quint16 value) {
        if (m_levelBEdit)
            m_levelBEdit->setValue(static_cast<qint16>(value));
    });

    // 6. 读取 AB 电平
    m_comm->requestRegisterRead(0x03EF, [this](quint16 value) {
        if (m_levelABEdit)
            m_levelABEdit->setValue(static_cast<qint16>(value));
    });

    // 7. 读取最小精度（寄存器值通常为 1、10、100）
    m_comm->requestRegisterRead(0x03F1, [this](quint16 value) {
        if (m_precisionEdit) {
            int idx = m_precisionEdit->findData(static_cast<int>(value));
            if (idx >= 0)
                m_precisionEdit->setCurrentIndex(idx);
        }
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

    // 写入已缓冲的数据
    for (int i = 0; i < m_time.size(); ++i) {
        QString dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
        .arg(m_time.at(i), 0, 'f', 3)
            .arg(m_rawA.at(i), 0, 'f', 4)
            .arg(m_filtA.at(i), 0, 'f', 4)
            .arg(m_rawB.at(i), 0, 'f', 4)
            .arg(m_filtB.at(i), 0, 'f', 4)
            .arg(m_rawAB.at(i), 0, 'f', 4)
            .arg(m_filtAB.at(i), 0, 'f', 4);
        m_saveStream << dataLine << "\n";
    }

    // 启动自动停止定时器（分钟转毫秒）
    if (durationMinutes > 0) {
        m_autoStopTimer->start(durationMinutes * 60 * 1000);
    }
}

void MonitorTab::stopDataSave()
{
    m_isSaving = false;
    m_autoStopTimer->stop();   // 停止自动停止定时器（无论是否已触发）
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
// 绘图与显示控制
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

    // A通道
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

    // B通道
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

    // AB通道
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