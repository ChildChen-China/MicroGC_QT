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

//-------------------------------------------------------------
// SignalPlotPanel 成员函数实现
//-------------------------------------------------------------
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
            // m_plot->yAxis->rescale(true);
        }
        m_plot->replot();
    });

    connect(m_crosshairBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_plot->setCrosshairEnabled(checked);
    });

    connect(zoomXBtn, &QPushButton::clicked, this, [this]() {
        m_plot->setZoomMode(InteractivePlot::ZoomX);
    });
    connect(zoomYBtn, &QPushButton::clicked, this, [this]() {
        m_plot->setZoomMode(InteractivePlot::ZoomY);
    });
    connect(zoomXYBtn, &QPushButton::clicked, this, [this]() {
        m_plot->setZoomMode(InteractivePlot::ZoomXY);
    });
    connect(zoomRectBtn, &QPushButton::clicked, this, [this]() {
        m_plot->setZoomMode(InteractivePlot::ZoomRect);
    });
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
        // 首次显示：设置 X 轴为最近20秒，Y 轴自适应数据
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

//-------------------------------------------------------------
// SaveWorker 实现
//-------------------------------------------------------------
SaveWorker::SaveWorker(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &SaveWorker::stopSaving);
}

SaveWorker::~SaveWorker()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void SaveWorker::startSaving(const QString &fileName, int durationMinutes)
{
    if (m_file.isOpen()) stopSaving();
    m_file.setFileName(fileName);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "SaveWorker: 无法打开文件:" << fileName;
        return;
    }
    m_stream.setDevice(&m_file);
    m_stream << "时间 滤波前A 滤波后A 滤波前B 滤波后B 滤波前A-B 滤波后A-B\n";
    m_timer->start(durationMinutes * 60 * 1000);
}

void SaveWorker::stopSaving()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
    m_timer->stop();
}

void SaveWorker::appendData(const QString &dataLine)
{
    if (m_file.isOpen()) {
        m_stream << dataLine << "\n";

    } else {
    }
}

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
        if (m_filterWorker) {
            QMetaObject::invokeMethod(m_filterWorker, "processFilter", Qt::QueuedConnection,
                                      Q_ARG(QVector<double>, m_time),
                                      Q_ARG(QVector<double>, m_rawA),
                                      Q_ARG(QVector<double>, m_rawB),
                                      Q_ARG(QVector<double>, m_rawAB),
                                      Q_ARG(int, m_averagePoints));
        }
    }
}

//-------------------------------------------------------------
// FilterWorker 实现
//-------------------------------------------------------------
FilterWorker::FilterWorker(QObject *parent) : QObject(parent) {}

void FilterWorker::processFilter(const QVector<double> &time,
                                 const QVector<double> &rawA,
                                 const QVector<double> &rawB,
                                 const QVector<double> &rawAB,
                                 int averagePoints)
{
    int window = qMax(1, averagePoints);
    auto smooth = [window](const QVector<double> &input) -> QVector<double> {
        QVector<double> output(input.size());
        for (int i = 0; i < input.size(); ++i) {
            int start = qMax(0, i - window/2);
            int end = qMin(input.size()-1, i + window/2);
            double sum = 0;
            for (int j = start; j <= end; ++j) sum += input[j];
            output[i] = sum / (end - start + 1);
        }
        return output;
    };
    emit filterCompleted(smooth(rawA), smooth(rawB), smooth(rawAB));
}

//-------------------------------------------------------------
// MonitorTab 构造函数
//-------------------------------------------------------------
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
    m_saveThread(nullptr),
    m_saveWorker(nullptr),
    m_filterThread(nullptr),
    m_filterWorker(nullptr),
    m_comm(nullptr),
    m_feedbackTimer(new QTimer(this)),
    m_hasPendingTemp(false),
    m_hasPendingPowerA(false),
    m_hasPendingPowerB(false),
    m_hasPendingPrecision(false),
    m_hasPendingLevelA(false),
    m_hasPendingLevelB(false),
    m_hasPendingLevelAB(false),
    m_lastSavedIndex(0),
    m_plotTimer(new QTimer(this))
{


    auto *mainLayout = new QHBoxLayout(this);

    auto *leftGroup = new QGroupBox("TCD控制面板", this);
    leftGroup->setMaximumWidth(350);
    auto *leftLayout = new QVBoxLayout(leftGroup);
    setupControlPanel(leftLayout);
    mainLayout->addWidget(leftGroup, 1);

    auto *rightGroup = new QGroupBox("信号显示", this);
    auto *rightLayout = new QVBoxLayout(rightGroup);
    setupSignalPanel(rightLayout);
    mainLayout->addWidget(rightGroup, 3);
    m_startTime = QDateTime::currentMSecsSinceEpoch();


    m_feedbackTimer->setInterval(2000);
    connect(m_feedbackTimer, &QTimer::timeout, this, &MonitorTab::checkSettingFeedback);
    m_feedbackTimer->start();

    // 绘图刷新定时器：每200ms直接更新曲线（使用原始或已有滤波数据）
    m_plotTimer->setInterval(50);
    connect(m_plotTimer, &QTimer::timeout, this, [this]() { updatePlots(false); });
    m_plotTimer->start();

    QTimer *filterTimer = new QTimer(this);
    filterTimer->setInterval(50);
    connect(filterTimer, &QTimer::timeout, this, [this]() {
        if (m_filterWorker) {
            QMetaObject::invokeMethod(m_filterWorker, "processFilter", Qt::QueuedConnection,
                                      Q_ARG(QVector<double>, m_time),
                                      Q_ARG(QVector<double>, m_rawA),
                                      Q_ARG(QVector<double>, m_rawB),
                                      Q_ARG(QVector<double>, m_rawAB),
                                      Q_ARG(int, m_averagePoints));
        }
    });
    filterTimer->start();
}

MonitorTab::~MonitorTab()
{
    if (m_saveThread && m_saveThread->isRunning()) {
        m_saveThread->quit();
        m_saveThread->wait();
    }
    if (m_filterThread && m_filterThread->isRunning()) {
        m_filterThread->quit();
        m_filterThread->wait();
    }
}


void MonitorTab::setCommunication(Communication *comm)
{
    m_comm = comm;
    if (m_comm) {
        connect(m_comm, &Communication::fastDataUpdated, this, &MonitorTab::onDataUpdated);
    }
}

void MonitorTab::onFastDataUpdated()
{
    if (!m_comm) return;

    double rawA = m_comm->tcdVoltageA();
    double rawB = m_comm->tcdVoltageB();
    double rawAB = m_comm->tcdVoltageAB();

    m_rawA.append(rawA);
    m_rawB.append(rawB);
    m_rawAB.append(rawAB);
    m_time.append(QDateTime::currentMSecsSinceEpoch() / 1000.0);

    // 限制数据长度
    while (m_time.size() > m_collectPoints) {
        m_time.removeFirst();
        m_rawA.removeFirst();
        m_rawB.removeFirst();
        m_rawAB.removeFirst();
        if (!m_filtA.isEmpty()) m_filtA.removeFirst();
        if (!m_filtB.isEmpty()) m_filtB.removeFirst();
        if (!m_filtAB.isEmpty()) m_filtAB.removeFirst();
    }

    // 文件写入（仅在保存状态时）
    if (m_isSaving && m_saveWorker && !m_time.isEmpty()) {
        int i = m_time.size() - 1; // 只写最新数据点

        // 辅助函数：若滤波数据长度不足，则根据原始数据实时计算该点的滤波值
        auto calcFilterValue = [this](const QVector<double>& raw, int index) -> double {
            if (index < 0 || index >= raw.size()) return 0.0;
            int window = qMax(1, m_averagePoints);
            int start = qMax(0, index - window/2);
            int end = qMin(raw.size()-1, index + window/2);
            double sum = 0.0;
            for (int j = start; j <= end; ++j) sum += raw[j];
            return sum / (end - start + 1);
        };

        // 优先使用线程计算好的滤波数据，若长度不足则临时计算
        double filtA = (i < m_filtA.size()) ? m_filtA.at(i) : calcFilterValue(m_rawA, i);
        double filtB = (i < m_filtB.size()) ? m_filtB.at(i) : calcFilterValue(m_rawB, i);
        double filtAB = (i < m_filtAB.size()) ? m_filtAB.at(i) : calcFilterValue(m_rawAB, i);

        QString dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
                               .arg(m_time.at(i), 0, 'f', 3)
                               .arg(m_rawA.at(i), 0, 'f', 4)
                               .arg(filtA, 0, 'f', 4)
                               .arg(m_rawB.at(i), 0, 'f', 4)
                               .arg(filtB, 0, 'f', 4)
                               .arg(m_rawAB.at(i), 0, 'f', 4)
                               .arg(filtAB, 0, 'f', 4);

        QMetaObject::invokeMethod(m_saveWorker, "appendData", Qt::QueuedConnection,
                                  Q_ARG(QString, dataLine));
    }
}
void MonitorTab::onSlowDataUpdated()
{
    if (!m_comm) return;
    double temp = m_comm->tcdTemperature();
    if (m_tempValueLabel) m_tempValueLabel->setText(QString("%1 ℃").arg(temp, 0, 'f', 2));
    // 其他慢速参数通过ControlTab和OtherTab的updateRealtimeLabels更新，或在此更新相关标签
}



void MonitorTab::setupControlPanel(QVBoxLayout *layout)
{
    QWidget *parent = layout->parentWidget();

    // 第一大项：基本控制
    auto *basicGroup = new QGroupBox("基本控制", parent);
    auto *basicLayout = new QVBoxLayout(basicGroup);

    // 使能按钮（替代原来的复选框）
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

    // 刷新按钮
    QPushButton *refreshBtn = new QPushButton("刷新", basicGroup);
    connect(refreshBtn, &QPushButton::clicked, this, &MonitorTab::refreshParameters);

    auto *row1 = new QHBoxLayout;
    row1->addWidget(m_enableCheck);
    row1->addWidget(refreshBtn);
    row1->addStretch();
    basicLayout->addLayout(row1);

    auto *row2 = new QHBoxLayout;
    row2->addWidget(new QLabel("读取温度:", basicGroup));
    m_tempValueLabel = new QLabel("-- ℃", basicGroup);
    row2->addWidget(m_tempValueLabel);
    row2->addStretch();
    basicLayout->addLayout(row2);

    auto *row3 = new QHBoxLayout;
    row3->addWidget(new QLabel("TCD温度设置:", basicGroup));
    m_tempEdit = new QSpinBox(basicGroup);   // QSpinBox
    m_tempEdit->setRange(-100, 500);
    m_tempEdit->setSuffix(" ℃");
    QPushButton *setTempBtn = new QPushButton("设置", basicGroup);
    row3->addWidget(m_tempEdit);
    row3->addWidget(setTempBtn);
    basicLayout->addLayout(row3);

    layout->addWidget(basicGroup);

    connect(setTempBtn, &QPushButton::clicked, this, &MonitorTab::setTemperature);

    // 第二大项：灯丝功率
    auto *powerGroup = new QGroupBox("灯丝功率", parent);
    auto *powerLayout = new QVBoxLayout(powerGroup);

    auto *powerRowA = new QHBoxLayout;
    powerRowA->addWidget(new QLabel("灯丝功率A:", powerGroup));
    m_powerAEdit = new QSpinBox(powerGroup);   // QSpinBox
    m_powerAEdit->setRange(0, 100);
    m_powerAEdit->setSuffix(" %");
    QPushButton *setPowerABtn = new QPushButton("设置", powerGroup);
    powerRowA->addWidget(m_powerAEdit);
    powerRowA->addWidget(setPowerABtn);
    powerLayout->addLayout(powerRowA);

    auto *powerRowB = new QHBoxLayout;
    powerRowB->addWidget(new QLabel("灯丝功率B:", powerGroup));
    m_powerBEdit = new QSpinBox(powerGroup);   // QSpinBox
    m_powerBEdit->setRange(0, 100);
    m_powerBEdit->setSuffix(" %");
    QPushButton *setPowerBBtn = new QPushButton("设置", powerGroup);
    powerRowB->addWidget(m_powerBEdit);
    powerRowB->addWidget(setPowerBBtn);
    powerLayout->addLayout(powerRowB);

    layout->addWidget(powerGroup);

    connect(setPowerABtn, &QPushButton::clicked, this, &MonitorTab::setPowerA);
    connect(setPowerBBtn, &QPushButton::clicked, this, &MonitorTab::setPowerB);

    // 第三大项：电平设置
    auto *levelGroup = new QGroupBox("电平设置", parent);
    auto *levelLayout = new QVBoxLayout(levelGroup);

    auto *levelRowA = new QHBoxLayout;
    levelRowA->addWidget(new QLabel("A电平(mV):", levelGroup));
    m_levelAEdit = new QSpinBox(levelGroup);   // QSpinBox
    m_levelAEdit->setRange(-1000, 1000);
    m_levelAEdit->setSuffix(" mV");
    QPushButton *setLevelABtn = new QPushButton("设置", levelGroup);
    levelRowA->addWidget(m_levelAEdit);
    levelRowA->addWidget(setLevelABtn);
    levelLayout->addLayout(levelRowA);

    auto *levelRowB = new QHBoxLayout;
    levelRowB->addWidget(new QLabel("B电平(mV):", levelGroup));
    m_levelBEdit = new QSpinBox(levelGroup);   // QSpinBox
    m_levelBEdit->setRange(-1000, 1000);
    m_levelBEdit->setSuffix(" mV");
    QPushButton *setLevelBBtn = new QPushButton("设置", levelGroup);
    levelRowB->addWidget(m_levelBEdit);
    levelRowB->addWidget(setLevelBBtn);
    levelLayout->addLayout(levelRowB);

    auto *levelRowAB = new QHBoxLayout;
    levelRowAB->addWidget(new QLabel("AB电平(mV):", levelGroup));
    m_levelABEdit = new QSpinBox(levelGroup);   // QSpinBox
    m_levelABEdit->setRange(-1000, 1000);
    m_levelABEdit->setSuffix(" mV");
    QPushButton *setLevelABBtn = new QPushButton("设置", levelGroup);
    levelRowAB->addWidget(m_levelABEdit);
    levelRowAB->addWidget(setLevelABBtn);
    levelLayout->addLayout(levelRowAB);

    auto *precisionRow = new QHBoxLayout;
    precisionRow->addWidget(new QLabel("精度:", levelGroup));
    m_precisionEdit = new QComboBox(levelGroup);   // QComboBox
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

    // 文件保存
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

void MonitorTab::onDataUpdated()
{
    if (!m_comm) return;

    // 使用成员变量 m_startTime 计算相对时间（秒）
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

    // ===== 因果滤波（只使用过去和当前数据，保证滤波值稳定）=====
    int window = qMax(1, m_averagePoints);
    auto smooth = [window](const QVector<double> &input) -> QVector<double> {
        QVector<double> output(input.size());
        for (int i = 0; i < input.size(); ++i) {
            int start = qMax(0, i - window + 1);  // 只包含当前点及之前的 window-1 个点
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

    int idx = m_time.size() - 1;

    // 更新界面显示（使用原始或滤波数据）
    updatePlots(false);

    // 文件写入（仅在保存激活且文件已打开）
    if (m_isSaving && m_saveFile.isOpen() && !m_time.isEmpty()) {
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

void MonitorTab::onFilterCompleted(const QVector<double> &filtA,
                                   const QVector<double> &filtB,
                                   const QVector<double> &filtAB)
{
    // 仅更新滤波结果，绘图和文件保存分别由定时器和快速数据槽处理
    if (filtA.size() != m_time.size() ||
        filtB.size() != m_time.size() ||
        filtAB.size() != m_time.size()) {
        return;
    }

    m_filtA = filtA;
    m_filtB = filtB;
    m_filtAB = filtAB;
}


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

    // ========== A通道 ==========
    if (m_panelA->isPaused()) {
        // 暂停状态：仅当强制刷新且有快照时使用快照
        if (force && m_panelA->hasSnapshot()) {
            QVector<double> x = m_panelA->snapshotX();
            QVector<double> y = m_showFiltA ? m_panelA->snapshotFiltY() : m_panelA->snapshotRawY();
            if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
                m_panelA->setData(x, y);
        }
        // 否则不更新，保持当前画面
    } else {
        // 非暂停：正常实时更新
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltA ? m_filtA : m_rawA;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelA->setData(x, y);
    }

    // ========== B通道 ==========
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

    // ========== AB通道 ==========
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


// 设置相关槽函数
void MonitorTab::setTemperature()
{
    if (!m_comm) return;
    int val = m_tempEdit->value();                       // QSpinBox 整数
    m_comm->setTcdTemperature(static_cast<quint16>(val));
    m_pendingTemp = val;
    m_hasPendingTemp = true;
    emit logMessage("TCD", QString("TCD温度设置已发送: %1 ℃").arg(val));
}

void MonitorTab::setPowerA()
{
    if (!m_comm) return;
    int val = m_powerAEdit->value();
    m_comm->setLampPowerA(static_cast<quint16>(val));
    m_pendingPowerA = val;
    m_hasPendingPowerA = true;
    emit logMessage("TCD", QString("灯丝功率A设置已发送: %1 %").arg(val));
}

void MonitorTab::setPowerB()
{
    if (!m_comm) return;
    int val = m_powerBEdit->value();
    m_comm->setLampPowerB(static_cast<quint16>(val));
    m_pendingPowerB = val;
    m_hasPendingPowerB = true;
    emit logMessage("TCD", QString("灯丝功率B设置已发送: %1 %").arg(val));
}

void MonitorTab::setLevelA()
{
    if (!m_comm) return;
    int val = m_levelAEdit->value();
    m_comm->setChannelAVoltage(static_cast<quint16>(val));
    m_pendingLevelA = val;
    m_hasPendingLevelA = true;
    emit logMessage("TCD", QString("A电平设置已发送: %1 mV").arg(val));
}

void MonitorTab::setLevelB()
{
    if (!m_comm) return;
    int val = m_levelBEdit->value();
    m_comm->setChannelBVoltage(static_cast<quint16>(val));
    m_pendingLevelB = val;
    m_hasPendingLevelB = true;
    emit logMessage("TCD", QString("B电平设置已发送: %1 mV").arg(val));
}

void MonitorTab::setLevelAB()
{
    if (!m_comm) return;
    int val = m_levelABEdit->value();
    m_comm->setChannelABVoltage(static_cast<quint16>(val));
    m_pendingLevelAB = val;
    m_hasPendingLevelAB = true;
    emit logMessage("TCD", QString("AB电平设置已发送: %1 mV").arg(val));
}

void MonitorTab::setPrecision()
{
    if (!m_comm) return;
    int val = m_precisionEdit->currentData().toInt();  // QComboBox 整数值
    m_comm->setPrecision(static_cast<quint16>(val));
    m_pendingPrecision = val;
    m_hasPendingPrecision = true;
    emit logMessage("TCD", QString("最小精度设置已发送: 寄存器值 %1").arg(val));
}

void MonitorTab::applyGlobalParameters()
{
    QTimer::singleShot(0,    this, [this]() { setTemperature(); });
    QTimer::singleShot(200,  this, [this]() { setPowerA(); });
    QTimer::singleShot(400,  this, [this]() { setPowerB(); });
    QTimer::singleShot(600,  this, [this]() { setLevelA(); });
    QTimer::singleShot(800,  this, [this]() { setLevelB(); });
    QTimer::singleShot(1000, this, [this]() { setLevelAB(); });
    QTimer::singleShot(1200, this, [this]() { setPrecision(); });
}

void MonitorTab::setDetectorEnabled(bool enabled)
{
    if (!m_enableCheck) return;

    // 临时阻止信号，避免触发手动点击时的命令发送逻辑
    m_enableCheck->blockSignals(true);
    m_enableCheck->setChecked(enabled);
    m_enableCheck->setText(enabled ? "关闭检测器" : "开启检测器");
    m_enableCheck->setStyleSheet(enabled ? "background-color: red; color: white;" :
                                     "background-color: gray; color: white;");
    m_enableCheck->blockSignals(false);
}

void MonitorTab::checkSettingFeedback()
{
    if (!m_comm) return;

    // 使用静态集合记录正在验证的参数，防止并发重复验证
    static QSet<QString> verifyingParams;

    auto startVerify = [this](const QString &paramName,
                              quint16 address,
                              quint16 expected,
                              bool &pendingFlag,
                              int &retryCount,
                              const std::function<void()> &resendFunc)
    {
        if (!pendingFlag || verifyingParams.contains(paramName))
            return;  // 无需验证或已在验证中

        verifyingParams.insert(paramName);   // 标记正在验证

        m_comm->requestRegisterRead(address, [=, &pendingFlag, &retryCount](quint16 actual) {
            verifyingParams.remove(paramName);   // 验证完成，移除标记

            if (!pendingFlag) return;   // 可能已被其他逻辑取消

            if (actual == expected) {
                pendingFlag = false;
                retryCount = 0;
                emit logMessage("TCD", QString("%1 设置成功").arg(paramName));
            } else {
                if (retryCount < 3) {
                    resendFunc();
                    retryCount++;
                    emit logMessage("TCD", QString("%1 设置失败，正在重试 (%1/3)").arg(paramName).arg(retryCount));
                } else {
                    pendingFlag = false;
                    retryCount = 0;
                    emit logMessage("TCD", QString("%1 设置失败，已超过最大重试次数").arg(paramName));
                }
            }
        });
    };

    // 各参数独立验证
    startVerify("TCD温度", 0x03E8, m_pendingTemp, m_hasPendingTemp, m_retryTemp,
                [this]() { m_comm->setTcdTemperature(m_pendingTemp); });
    startVerify("灯丝功率A", 0x03E9, m_pendingPowerA, m_hasPendingPowerA, m_retryPowerA,
                [this]() { m_comm->setLampPowerA(m_pendingPowerA); });
    startVerify("灯丝功率B", 0x03EA, m_pendingPowerB, m_hasPendingPowerB, m_retryPowerB,
                [this]() { m_comm->setLampPowerB(m_pendingPowerB); });
    startVerify("最小精度", 0x03F1, m_pendingPrecision, m_hasPendingPrecision, m_retryPrecision,
                [this]() { m_comm->setPrecision(m_pendingPrecision); });
    startVerify("A电平", 0x03ED, m_pendingLevelA, m_hasPendingLevelA, m_retryLevelA,
                [this]() { m_comm->setChannelAVoltage(m_pendingLevelA); });
    startVerify("B电平", 0x03EE, m_pendingLevelB, m_hasPendingLevelB, m_retryLevelB,
                [this]() { m_comm->setChannelBVoltage(m_pendingLevelB); });
    startVerify("AB电平", 0x03EF, m_pendingLevelAB, m_hasPendingLevelAB, m_retryLevelAB,
                [this]() { m_comm->setChannelABVoltage(m_pendingLevelAB); });
}

void MonitorTab::refreshParameters()
{
    if (!m_comm) return;

    // 更新读取温度显示
    double temp = m_comm->tcdTemperature();
    if (m_tempValueLabel) m_tempValueLabel->setText(QString("%1 ℃").arg(temp, 0, 'f', 2));

    // 以下尝试读取命令区寄存器，但协议未提供，我们暂时只刷新温度。
    // 如果有需要，可在 Communication 中添加读取命令区的方法。
    emit logMessage("TCD", "刷新参数（当前仅刷新温度）");
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

void MonitorTab::startAutoSave()
{
    if (m_isSaving) {
        // 已有保存任务在运行，忽略新的请求
        return;
    }

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
    if (m_saveFile.isOpen()) m_saveFile.close();

    m_saveFile.setFileName(fileName);
    if (!m_saveFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "无法打开文件:" << fileName;
        m_isSaving = false;
        return;
    }
    m_saveStream.setDevice(&m_saveFile);
    m_saveStream << "时间 滤波前A 滤波后A 滤波前B 滤波后B 滤波前A-B 滤波后A-B\n";

    // 写入已缓冲的所有数据（此时滤波值已固定，直接使用）
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
}

void MonitorTab::stopDataSave()
{
    m_isSaving = false;
    if (m_saveFile.isOpen()) {
        m_saveStream.flush();
        m_saveFile.close();
    }
}



void MonitorTab::setChannelAVisible(bool visible) { if (m_panelA) m_panelA->setVisible(visible); }
void MonitorTab::setChannelBVisible(bool visible) { if (m_panelB) m_panelB->setVisible(visible); }
void MonitorTab::setChannelABVisible(bool visible) { if (m_panelAB) m_panelAB->setVisible(visible); }

void MonitorTab::setCollectPoints(int points) { m_collectPoints = points; updateDisplayLength(); }
void MonitorTab::setAveragePoints(int points) { m_averagePoints = points; }

void MonitorTab::toggleFilterA()
{
    m_showFiltA = !m_showFiltA;
    m_filterBtnA->setText(m_showFiltA ? "显示滤波后" : "显示滤波前");
    updatePlots(true);
}

void MonitorTab::toggleFilterB()
{
    m_showFiltB = !m_showFiltB;
    m_filterBtnB->setText(m_showFiltB ? "显示滤波后" : "显示滤波前");
    updatePlots(true);
}

void MonitorTab::toggleFilterAB()
{
    m_showFiltAB = !m_showFiltAB;
    m_filterBtnAB->setText(m_showFiltAB ? "显示滤波后" : "显示滤波前");
    updatePlots(true);
}