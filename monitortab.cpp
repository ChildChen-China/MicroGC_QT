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
        m_plot->resetView(0, 5, 0, 5);
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
    m_x = x;
    m_y = y;
    applyInversion();
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
        qWarning() << "无法打开文件:" << fileName;
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
    m_lastSavedIndex(0)
{
    initSaveThread();
    initFilterThread();

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

    m_feedbackTimer->setInterval(1000);
    connect(m_feedbackTimer, &QTimer::timeout, this, &MonitorTab::checkSettingFeedback);
    m_feedbackTimer->start();
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
        connect(m_comm, &Communication::dataUpdated, this, &MonitorTab::onDataUpdated);
    }
}

void MonitorTab::initSaveThread()
{
    m_saveThread = new QThread(this);
    m_saveWorker = new SaveWorker();
    m_saveWorker->moveToThread(m_saveThread);
    connect(m_saveThread, &QThread::finished, m_saveWorker, &QObject::deleteLater);
    m_saveThread->start();
}

void MonitorTab::initFilterThread()
{
    m_filterThread = new QThread(this);
    m_filterWorker = new FilterWorker();
    m_filterWorker->moveToThread(m_filterThread);
    connect(m_filterThread, &QThread::finished, m_filterWorker, &QObject::deleteLater);
    connect(m_filterWorker, &FilterWorker::filterCompleted,
            this, &MonitorTab::onFilterCompleted, Qt::QueuedConnection);
    m_filterThread->start();
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

// 数据更新入口（由 Communication 触发）
void MonitorTab::onDataUpdated()
{
    if (!m_comm) return;

    // 使用静态变量记录起始时间，生成相对秒数
    static qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    double timeSec = (QDateTime::currentMSecsSinceEpoch() - startTime) / 1000.0;

    double rawA = m_comm->tcdVoltageA();
    double rawB = m_comm->tcdVoltageB();
    double rawAB = m_comm->tcdVoltageAB();
    double temp = m_comm->tcdTemperature();

    m_time.append(timeSec);
    m_rawA.append(rawA);
    m_rawB.append(rawB);
    m_rawAB.append(rawAB);

    if (m_tempValueLabel) m_tempValueLabel->setText(QString("%1 ℃").arg(temp, 0, 'f', 2));

    updateDisplayLength();

    if (m_filterWorker) {
        QMetaObject::invokeMethod(m_filterWorker, "processFilter", Qt::QueuedConnection,
                                  Q_ARG(QVector<double>, m_time),
                                  Q_ARG(QVector<double>, m_rawA),
                                  Q_ARG(QVector<double>, m_rawB),
                                  Q_ARG(QVector<double>, m_rawAB),
                                  Q_ARG(int, m_averagePoints));
    }

    updatePlots(false);

    if (m_saveWorker) {
        QString dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
        .arg(timeSec, 0, 'f', 3)
            .arg(rawA, 0, 'f', 4)
            .arg(m_filtA.isEmpty() ? 0 : m_filtA.last(), 0, 'f', 4)
            .arg(rawB, 0, 'f', 4)
            .arg(m_filtB.isEmpty() ? 0 : m_filtB.last(), 0, 'f', 4)
            .arg(rawAB, 0, 'f', 4)
            .arg(m_filtAB.isEmpty() ? 0 : m_filtAB.last(), 0, 'f', 4);
        QMetaObject::invokeMethod(m_saveWorker, "appendData", Qt::QueuedConnection,
                                  Q_ARG(QString, dataLine));
    }
}

void MonitorTab::onFilterCompleted(const QVector<double> &filtA,
                                   const QVector<double> &filtB,
                                   const QVector<double> &filtAB)
{
    if (filtA.size() != m_time.size() ||
        filtB.size() != m_time.size() ||
        filtAB.size() != m_time.size()) {
        return;
    }

    m_filtA = filtA;
    m_filtB = filtB;
    m_filtAB = filtAB;
    updatePlots(false);
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

    // A通道
    if (force || !m_panelA->isPaused()) {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltA ? m_filtA : m_rawA;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelA->setData(x, y);
    } else if (force && m_panelA->hasSnapshot()) {
        QVector<double> x = m_panelA->snapshotX();
        QVector<double> y = m_showFiltA ? m_panelA->snapshotFiltY() : m_panelA->snapshotRawY();
        if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
            m_panelA->setData(x, y);
    }

    // B通道
    if (force || !m_panelB->isPaused()) {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltB ? m_filtB : m_rawB;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelB->setData(x, y);
    } else if (force && m_panelB->hasSnapshot()) {
        QVector<double> x = m_panelB->snapshotX();
        QVector<double> y = m_showFiltB ? m_panelB->snapshotFiltY() : m_panelB->snapshotRawY();
        if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
            m_panelB->setData(x, y);
    }

    // AB通道
    if (force || !m_panelAB->isPaused()) {
        QVector<double> x = m_time;
        QVector<double> y = m_showFiltAB ? m_filtAB : m_rawAB;
        if (y.size() == x.size() && !y.isEmpty())
            m_panelAB->setData(x, y);
    } else if (force && m_panelAB->hasSnapshot()) {
        QVector<double> x = m_panelAB->snapshotX();
        QVector<double> y = m_showFiltAB ? m_panelAB->snapshotFiltY() : m_panelAB->snapshotRawY();
        if (!x.isEmpty() && !y.isEmpty() && x.size() == y.size())
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

void MonitorTab::checkSettingFeedback()
{
    if (!m_comm) return;

    // 辅助宏，简化重复代码
    auto verifyAndResend = [this](quint16 address, quint16 expected,
                                  bool &pendingFlag, int &retryCounter,
                                  const std::function<void()> &resendFunc,
                                  const QString &name) {
        if (!pendingFlag) return;
        m_comm->requestRegisterRead(address, [=, &pendingFlag, &retryCounter](quint16 actual) {
            if (actual == expected) {
                pendingFlag = false;
                retryCounter = 0;
                emit logMessage("TCD", QString("%1设置成功").arg(name));
            } else {
                if (retryCounter < 3) {
                    resendFunc();
                    retryCounter++;
                    emit logMessage("TCD", QString("%1设置失败，重试%1/3").arg(name).arg(retryCounter));
                } else {
                    pendingFlag = false;
                    retryCounter = 0;
                    emit logMessage("TCD", QString("%1设置失败，已达最大重试次数").arg(name));
                }
            }
        });
    };

    // 温度
    verifyAndResend(0x03E8, m_pendingTemp, m_hasPendingTemp, m_retryTemp,
                    [this]() { m_comm->setTcdTemperature(m_pendingTemp); }, "TCD温度");

    // 灯丝功率A
    verifyAndResend(0x03E9, m_pendingPowerA, m_hasPendingPowerA, m_retryPowerA,
                    [this]() { m_comm->setLampPowerA(m_pendingPowerA); }, "灯丝功率A");

    // 灯丝功率B
    verifyAndResend(0x03EA, m_pendingPowerB, m_hasPendingPowerB, m_retryPowerB,
                    [this]() { m_comm->setLampPowerB(m_pendingPowerB); }, "灯丝功率B");

    // 最小精度
    verifyAndResend(0x03F1, m_pendingPrecision, m_hasPendingPrecision, m_retryPrecision,
                    [this]() { m_comm->setPrecision(m_pendingPrecision); }, "最小精度");

    // A电平
    verifyAndResend(0x03ED, m_pendingLevelA, m_hasPendingLevelA, m_retryLevelA,
                    [this]() { m_comm->setChannelAVoltage(m_pendingLevelA); }, "A电平");

    // B电平
    verifyAndResend(0x03EE, m_pendingLevelB, m_hasPendingLevelB, m_retryLevelB,
                    [this]() { m_comm->setChannelBVoltage(m_pendingLevelB); }, "B电平");

    // AB电平
    verifyAndResend(0x03EF, m_pendingLevelAB, m_hasPendingLevelAB, m_retryLevelAB,
                    [this]() { m_comm->setChannelABVoltage(m_pendingLevelAB); }, "AB电平");
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
    m_lastSavedIndex = 0;
    if (m_saveWorker) {
        QMetaObject::invokeMethod(m_saveWorker, "startSaving", Qt::QueuedConnection,
                                  Q_ARG(QString, fileName), Q_ARG(int, durationMinutes));
    }
}

void MonitorTab::stopDataSave()
{
    if (m_saveWorker) {
        QMetaObject::invokeMethod(m_saveWorker, "stopSaving", Qt::QueuedConnection);
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