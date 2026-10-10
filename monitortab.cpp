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
#include <QScrollArea>

//==========================================================
// SignalPlotPanel
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
    m_plot->yAxis->setLabel("Amplitude(uV)");
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
// MonitorTab
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
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // ---- 左侧控制面板 ----
    auto *leftGroup = new QGroupBox("TCD控制面板");
    leftGroup->setMinimumWidth(320);
    leftGroup->setMaximumWidth(400);
    auto *leftLayout = new QVBoxLayout(leftGroup);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    setupControlPanel(leftLayout);

    auto *leftScroll = new QScrollArea(this);
    leftScroll->setWidget(leftGroup);
    leftScroll->setWidgetResizable(true);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setMinimumWidth(340);
    mainLayout->addWidget(leftScroll, 1);

    // ---- 右侧信号显示 ----
    auto *rightGroup = new QGroupBox("信号显示");
    auto *rightLayout = new QVBoxLayout(rightGroup);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    setupSignalPanel(rightLayout);

    auto *rightScroll = new QScrollArea(this);
    rightScroll->setWidget(rightGroup);
    rightScroll->setWidgetResizable(true);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    rightScroll->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(rightScroll, 3);

    // 反馈定时器
    m_feedbackTimer->setInterval(2000);
    connect(m_feedbackTimer, &QTimer::timeout, this, &MonitorTab::checkSettingFeedback);
    m_feedbackTimer->start();

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

void MonitorTab::setTcdPowered(bool powered)
{
    if (m_tcdPowered == powered) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (powered) {
        m_tcdLastTick = now;
    } else {
        if (m_tcdLastTick > 0) {
            m_tcdActiveMs += now - m_tcdLastTick;
            m_tcdLastTick = 0;
        }
    }
    m_tcdPowered = powered;
}

//==========================================================
// 数据采集开始（从当前时刻）
//==========================================================
void MonitorTab::startDataSaveNow()
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

    m_isSaving = true;
    if (m_saveFile.isOpen()) {
        m_saveStream.flush();
        m_saveFile.close();
    }

    m_saveFile.setFileName(fullPath);
    if (!m_saveFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "无法打开文件:" << fullPath;
        m_isSaving = false;
        return;
    }
    m_saveStream.setDevice(&m_saveFile);
    m_saveStream << "时间 滤波前A 滤波后A 滤波前B 滤波后B 滤波前A-B 滤波后A-B\n";

    emit logMessage("文件操作", QString("开始保存数据至 %1").arg(fullPath));

    // 手动保存时长
    int durationMinutes = m_durationSpin->value();
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
        emit logMessage("文件操作", "停止写入数据文件");
    }
}

//==========================================================
// 极性反转
//==========================================================
void MonitorTab::startPolarityReversal()
{
    m_polarityActive = true;
    emit logMessage("TCD", "极性反转开启");
}

void MonitorTab::stopPolarityReversal()
{
    m_polarityActive = false;
    emit logMessage("TCD", "极性反转关闭");
}

//==========================================================
// 数据更新
//==========================================================
void MonitorTab::onDataUpdated()
{
    if (!m_comm) return;

    if (!m_tcdPowered) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double timeSec = (m_tcdActiveMs + (now - m_tcdLastTick)) / 1000.0;

    double rawA = m_comm->tcdVoltageA();
    double rawB = m_comm->tcdVoltageB();
    double rawAB = m_comm->tcdVoltageAB();

    // 应用极性反转（只影响新数据点）
    double dispA  = m_polarityActive ? -rawA  : rawA;
    double dispB  = m_polarityActive ? -rawB  : rawB;
    double dispAB = m_polarityActive ? -rawAB : rawAB;

    m_time.append(timeSec);
    m_rawA.append(rawA);
    m_rawB.append(rawB);
    m_rawAB.append(rawAB);
    m_dispA.append(dispA);
    m_dispB.append(dispB);
    m_dispAB.append(dispAB);

    while (m_time.size() > m_collectPoints) {
        m_time.removeFirst();
        m_rawA.removeFirst();
        m_rawB.removeFirst();
        m_rawAB.removeFirst();
        m_dispA.removeFirst();
        m_dispB.removeFirst();
        m_dispAB.removeFirst();
        if (!m_filtA.isEmpty()) m_filtA.removeFirst();
        if (!m_filtB.isEmpty()) m_filtB.removeFirst();
        if (!m_filtAB.isEmpty()) m_filtAB.removeFirst();
    }

    // 滤波基于显示值（已反转），保证显示和保存一致
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
    m_filtA = smooth(m_dispA);
    m_filtB = smooth(m_dispB);
    m_filtAB = smooth(m_dispAB);

    updatePlots(false);

    // 保存文件：写显示值（已应用反转），时间 6 位小数，电压 1 位小数
    if (m_isSaving && m_saveFile.isOpen() && !m_time.isEmpty()) {
        int idx = m_time.size() - 1;

        QString dataLine;
        if (qIsNaN(m_dispA.at(idx))) {
            dataLine = QString("%1 nan nan nan nan nan nan")
            .arg(m_time.at(idx), 0, 'f', 6);
        } else {
            dataLine = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(m_time.at(idx), 0, 'f', 6)
                .arg(m_dispA.at(idx), 0, 'f', 1)
                .arg(m_filtA.at(idx), 0, 'f', 1)
                .arg(m_dispB.at(idx), 0, 'f', 1)
                .arg(m_filtB.at(idx), 0, 'f', 1)
                .arg(m_dispAB.at(idx), 0, 'f', 1)
                .arg(m_filtAB.at(idx), 0, 'f', 1);
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
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("TCD温度:", basicGroup));
        m_tempEdit = new QSpinBox(basicGroup);
        m_tempEdit->setRange(0, 500);
        m_tempEdit->setSuffix(" ℃");
        QPushButton *btn = new QPushButton("设置", basicGroup);
        btn->setToolTip("TS 98.00\\r  设置TCD温度（发送值 = 输入 ×100）");
        row->addWidget(m_tempEdit);
        row->addWidget(btn);
        basicLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setTemperature);
    }

    // ZS
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("调零动作:", basicGroup));
        m_zsEdit = new QSpinBox(basicGroup);
        m_zsEdit->setRange(0, 4);
        QPushButton *btn = new QPushButton("设置", basicGroup);
        btn->setToolTip("1=ZS 默认调零，2=ZR 调A-B，3=ZB 调B，4=ZA 调A");
        row->addWidget(m_zsEdit);
        row->addWidget(btn);
        basicLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setZS);
    }

    // 精度
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("最小精度:", basicGroup));
        m_precisionEdit = new QComboBox(basicGroup);
        m_precisionEdit->addItem("0.001", 1);
        m_precisionEdit->addItem("0.01", 10);
        m_precisionEdit->addItem("0.05", 50);
        m_precisionEdit->addItem("0.1", 100);
        m_precisionEdit->addItem("1", 1000);
        QPushButton *btn = new QPushButton("设置", basicGroup);
        btn->setToolTip("PF 0.05\\r   设置最小精度（1~1000，0.001/计数）");
        row->addWidget(m_precisionEdit);
        row->addWidget(btn);
        basicLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setPrecision);
    }

    // RF
    {
        auto *row = new QHBoxLayout;
        m_rfBtn = new QPushButton("复位TCD错误标志 (RF)", basicGroup);
        m_rfBtn->setToolTip("每次合法写1，MCU发送 ASCII \"RF\\r\"");
        row->addWidget(m_rfBtn);
        basicLayout->addLayout(row);
        connect(m_rfBtn, &QPushButton::clicked, this, &MonitorTab::setRF);
    }

    layout->addWidget(basicGroup);

    // ---------- FA/FB 功率 ----------
    auto *powerGroup = new QGroupBox("FA/FB功率", parent);
    auto *powerLayout = new QVBoxLayout(powerGroup);

    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("FA功率:", powerGroup));
        m_powerAEdit = new QSpinBox(powerGroup);
        m_powerAEdit->setRange(0, 100);
        m_powerAEdit->setSuffix(" %");
        QPushButton *btn = new QPushButton("设置", powerGroup);
        btn->setToolTip("FA 63\\r 设置灯丝A功率 (0~100)");
        row->addWidget(m_powerAEdit);
        row->addWidget(btn);
        powerLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setPowerA);
    }
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("FB功率:", powerGroup));
        m_powerBEdit = new QSpinBox(powerGroup);
        m_powerBEdit->setRange(0, 100);
        m_powerBEdit->setSuffix(" %");
        QPushButton *btn = new QPushButton("设置", powerGroup);
        btn->setToolTip("FB 63\\r  设置灯丝B功率 (0~100)");
        row->addWidget(m_powerBEdit);
        row->addWidget(btn);
        powerLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setPowerB);
    }

    layout->addWidget(powerGroup);

    // ---------- ZA/ZB/ZR ----------
    auto *levelGroup = new QGroupBox("电平设置", parent);
    auto *levelLayout = new QVBoxLayout(levelGroup);

    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("ZA电压:", levelGroup));
        m_levelAEdit = new QSpinBox(levelGroup);
        m_levelAEdit->setRange(-1200, 1200);
        m_levelAEdit->setSuffix(" mV");
        QPushButton *btn = new QPushButton("设置", levelGroup);
        btn->setToolTip("ZA ±x.xxx\\r （-1200~+1200 mV）");
        row->addWidget(m_levelAEdit);
        row->addWidget(btn);
        levelLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setLevelA);
    }
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("ZB电压:", levelGroup));
        m_levelBEdit = new QSpinBox(levelGroup);
        m_levelBEdit->setRange(-1200, 1200);
        m_levelBEdit->setSuffix(" mV");
        QPushButton *btn = new QPushButton("设置", levelGroup);
        btn->setToolTip("ZB ±x.xxx\\r （-1200~+1200 mV）");
        row->addWidget(m_levelBEdit);
        row->addWidget(btn);
        levelLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setLevelB);
    }
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("ZR电压:", levelGroup));
        m_levelABEdit = new QSpinBox(levelGroup);
        m_levelABEdit->setRange(-1200, 1200);
        m_levelABEdit->setSuffix(" mV");
        QPushButton *btn = new QPushButton("设置", levelGroup);
        btn->setToolTip("ZR ±x.xxx\\r （-1200~+1200 mV）");
        row->addWidget(m_levelABEdit);
        row->addWidget(btn);
        levelLayout->addLayout(row);
        connect(btn, &QPushButton::clicked, this, &MonitorTab::setLevelAB);
    }

    layout->addWidget(levelGroup);

    // ---------- 文件保存 ----------
    auto *fileGroup = new QGroupBox("文件保存", parent);
    auto *fileLayout = new QVBoxLayout(fileGroup);

    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("文件名:", fileGroup));
        m_fileNameEdit = new QLineEdit("data", fileGroup);
        row->addWidget(m_fileNameEdit);
        fileLayout->addLayout(row);
    }
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("保存时长:", fileGroup));
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

        row->addWidget(m_durationSpin);
        row->addStretch();
        fileLayout->addLayout(row);
    }
    {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel("路径:", fileGroup));
        m_pathEdit = new QLineEdit("./data", fileGroup);
        QPushButton *choosePathBtn = new QPushButton("选择路径", fileGroup);
        row->addWidget(m_pathEdit);
        row->addWidget(choosePathBtn);
        fileLayout->addLayout(row);
        connect(choosePathBtn, &QPushButton::clicked, this, &MonitorTab::choosePath);
    }
    {
        QPushButton *stopBtn = new QPushButton("停止", fileGroup);
        fileLayout->addWidget(stopBtn);
        connect(stopBtn, &QPushButton::clicked, this, &MonitorTab::stopFileSave);
    }

    layout->addWidget(fileGroup);
    layout->addStretch();
}

void MonitorTab::setupSignalPanel(QVBoxLayout *layout)
{
    QWidget *parent = layout->parentWidget();

    m_panelA = new SignalPlotPanel("A", parent);
    m_panelB = new SignalPlotPanel("B", parent);
    m_panelAB = new SignalPlotPanel("A-B", parent);

    m_panelA->setMinimumHeight(180);
    m_panelB->setMinimumHeight(180);
    m_panelAB->setMinimumHeight(180);

    m_filterBtnA = m_panelA->filterButton();
    m_filterBtnB = m_panelB->filterButton();
    m_filterBtnAB = m_panelAB->filterButton();

    connect(m_filterBtnA, &QPushButton::clicked, this, &MonitorTab::toggleFilterA);
    connect(m_filterBtnB, &QPushButton::clicked, this, &MonitorTab::toggleFilterB);
    connect(m_filterBtnAB, &QPushButton::clicked, this, &MonitorTab::toggleFilterAB);

    connect(m_panelA, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_dispA.size(), m_filtA.size()));
            m_panelA->setSnapshot(m_time.mid(0, len), m_dispA.mid(0, len), m_filtA.mid(0, len));
        } else {
            m_panelA->clearSnapshot();
        }
    });
    connect(m_panelB, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_dispB.size(), m_filtB.size()));
            m_panelB->setSnapshot(m_time.mid(0, len), m_dispB.mid(0, len), m_filtB.mid(0, len));
        } else {
            m_panelB->clearSnapshot();
        }
    });
    connect(m_panelAB, &SignalPlotPanel::pauseStateChanged, this, [this](bool paused) {
        if (paused) {
            int len = qMin(m_time.size(), qMin(m_dispAB.size(), m_filtAB.size()));
            m_panelAB->setSnapshot(m_time.mid(0, len), m_dispAB.mid(0, len), m_filtAB.mid(0, len));
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
        m_levelAEdit->setValue(value);
    } else if (key == "levelB") {
        m_levelBEdit->setValue(value);
    } else if (key == "levelAB") {
        m_levelABEdit->setValue(value);
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
    quint16 raw = static_cast<quint16>(val * 100);
    m_comm->setTcdTemperature(raw);
    m_pendingTemp = raw;
    m_hasPendingTemp = true;
    emit logMessage("TCD", QString("TCD温度设置已发送: %1 ℃ (raw=%2)").arg(val).arg(raw));
}

void MonitorTab::setPowerA()
{
    if (!m_comm) return;
    int val = m_powerAEdit->value();
    m_comm->setLampPowerA(static_cast<quint16>(val));
    m_pendingPowerA = static_cast<quint16>(val);
    m_hasPendingPowerA = true;
    emit logMessage("TCD", QString("FA功率设置已发送: %1 %").arg(val));
}

void MonitorTab::setPowerB()
{
    if (!m_comm) return;
    int val = m_powerBEdit->value();
    m_comm->setLampPowerB(static_cast<quint16>(val));
    m_pendingPowerB = static_cast<quint16>(val);
    m_hasPendingPowerB = true;
    emit logMessage("TCD", QString("FB功率设置已发送: %1 %").arg(val));
}

void MonitorTab::setLevelA()
{
    if (!m_comm) return;
    int val = m_levelAEdit->value();
    qint16 signedVal = static_cast<qint16>(val);
    m_comm->setChannelAVoltage(static_cast<quint16>(signedVal));
    m_pendingLevelA = static_cast<quint16>(signedVal);
    m_hasPendingLevelA = true;
    emit logMessage("TCD", QString("ZA电压设置已发送: %1 mV").arg(val));
}

void MonitorTab::setLevelB()
{
    if (!m_comm) return;
    int val = m_levelBEdit->value();
    qint16 signedVal = static_cast<qint16>(val);
    m_comm->setChannelBVoltage(static_cast<quint16>(signedVal));
    m_pendingLevelB = static_cast<quint16>(signedVal);
    m_hasPendingLevelB = true;
    emit logMessage("TCD", QString("ZB电压设置已发送: %1 mV").arg(val));
}

void MonitorTab::setLevelAB()
{
    if (!m_comm) return;
    int val = m_levelABEdit->value();
    qint16 signedVal = static_cast<qint16>(val);
    m_comm->setChannelABVoltage(static_cast<quint16>(signedVal));
    m_pendingLevelAB = static_cast<quint16>(signedVal);
    m_hasPendingLevelAB = true;
    emit logMessage("TCD", QString("ZR电压设置已发送: %1 mV").arg(val));
}

void MonitorTab::setZS()
{
    if (!m_comm) return;
    int val = m_zsEdit->value();
    if (val < 1 || val > 4) {
        emit logMessage("TCD", "调零动作值必须为 1~4");
        return;
    }
    m_comm->setZS(static_cast<quint16>(val));
    m_pendingZS = static_cast<quint16>(val);
    m_hasPendingZS = true;
    emit logMessage("TCD", QString("调零动作已发送: %1").arg(val));
}

void MonitorTab::setPrecision()
{
    if (!m_comm) return;
    int val = m_precisionEdit->currentData().toInt();
    m_comm->setPrecision(static_cast<quint16>(val));
    m_pendingPrecision = static_cast<quint16>(val);
    m_hasPendingPrecision = true;
    emit logMessage("TCD", QString("最小精度设置已发送: %1").arg(val));
}

void MonitorTab::setRF()
{
    if (!m_comm) return;
    m_comm->setRF(1);
    emit logMessage("TCD", "复位TCD错误标志已发送");
}

void MonitorTab::applyGlobalParameters()
{
    if (!m_comm) return;
    setTemperature();
    setLevelA();
    setLevelB();
    setLevelAB();
    setPrecision();
}

//==========================================================
// 反馈验证
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

    verify("ZA电压", 0x03ED, m_pendingLevelA, m_hasPendingLevelA, m_retryLevelA,
           [this]() { m_comm->setChannelAVoltage(m_pendingLevelA); });

    verify("ZB电压", 0x03EE, m_pendingLevelB, m_hasPendingLevelB, m_retryLevelB,
           [this]() { m_comm->setChannelBVoltage(m_pendingLevelB); });

    verify("ZR电压", 0x03EF, m_pendingLevelAB, m_hasPendingLevelAB, m_retryLevelAB,
           [this]() { m_comm->setChannelABVoltage(m_pendingLevelAB); });
}

//==========================================================
// 刷新
//==========================================================
void MonitorTab::refreshParameters()
{
    if (!m_comm) return;

    m_comm->requestRegisterRead(0x03E8, [this](quint16 v) {
        if (m_tempEdit) m_tempEdit->setValue(v / 100);
    });
    m_comm->requestRegisterRead(0x03E9, [this](quint16 v) {
        if (m_powerAEdit) m_powerAEdit->setValue(v);
    });
    m_comm->requestRegisterRead(0x03EA, [this](quint16 v) {
        if (m_powerBEdit) m_powerBEdit->setValue(v);
    });
    m_comm->requestRegisterRead(0x03ED, [this](quint16 v) {
        if (m_levelAEdit) m_levelAEdit->setValue(static_cast<qint16>(v));
    });
    m_comm->requestRegisterRead(0x03EE, [this](quint16 v) {
        if (m_levelBEdit) m_levelBEdit->setValue(static_cast<qint16>(v));
    });
    m_comm->requestRegisterRead(0x03EF, [this](quint16 v) {
        if (m_levelABEdit) m_levelABEdit->setValue(static_cast<qint16>(v));
    });
    m_comm->requestRegisterRead(0x03F1, [this](quint16 v) {
        if (m_precisionEdit) {
            int idx = m_precisionEdit->findData(static_cast<int>(v));
            if (idx >= 0) m_precisionEdit->setCurrentIndex(idx);
        }
    });

    emit logMessage("TCD", "刷新参数：已请求读取所有设置值");
}

//==========================================================
// 手动停止
//==========================================================
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
        m_dispA.removeFirst();
        m_dispB.removeFirst();
        m_dispAB.removeFirst();
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
        QVector<double> y = m_showFiltA ? m_filtA : m_dispA;
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
        QVector<double> y = m_showFiltB ? m_filtB : m_dispB;
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
        QVector<double> y = m_showFiltAB ? m_filtAB : m_dispAB;
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

void MonitorTab::setCollectPoints(int points)
{
    m_collectPoints = points;
    updateDisplayLength();

    // 自动持久化到 QSettings
    QSettings s("MyCompany", "MicroGC");
    s.setValue("global/collectPoints", points);
    s.sync();
}

void MonitorTab::setAveragePoints(int points)
{
    m_averagePoints = points;

    // 自动持久化到 QSettings
    QSettings s("MyCompany", "MicroGC");
    s.setValue("global/averagePoints", points);
    s.sync();
}