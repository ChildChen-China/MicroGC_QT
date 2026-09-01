#include "dataprocessing.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QRadioButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPen>
#include <QThread>
#include <QtMath>
#include <QDebug>

#include "interactiveplot.h"
#include "qcustomplot.h"

//-------------------------------------------------------------
// IntegralSettingsDialog 实现
//-------------------------------------------------------------
IntegralSettingsDialog::IntegralSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("积分设置");
    setMinimumWidth(350);

    auto *mainLayout = new QVBoxLayout(this);

    // 通道显示选择
    auto *chkGroup = new QGroupBox("显示通道", this);
    auto *chkLayout = new QGridLayout(chkGroup);
    m_chkRawA = new QCheckBox("A滤波前", chkGroup);
    m_chkFiltA = new QCheckBox("A滤波后", chkGroup);
    m_chkRawB = new QCheckBox("B滤波前", chkGroup);
    m_chkFiltB = new QCheckBox("B滤波后", chkGroup);
    m_chkRawAB = new QCheckBox("A-B滤波前", chkGroup);
    m_chkFiltAB = new QCheckBox("A-B滤波后", chkGroup);

    m_chkRawA->setChecked(true);
    m_chkFiltA->setChecked(true);
    m_chkRawB->setChecked(true);
    m_chkFiltB->setChecked(true);
    m_chkRawAB->setChecked(true);
    m_chkFiltAB->setChecked(true);

    chkLayout->addWidget(m_chkRawA, 0, 0);
    chkLayout->addWidget(m_chkFiltA, 0, 1);
    chkLayout->addWidget(m_chkRawB, 1, 0);
    chkLayout->addWidget(m_chkFiltB, 1, 1);
    chkLayout->addWidget(m_chkRawAB, 2, 0);
    chkLayout->addWidget(m_chkFiltAB, 2, 1);
    mainLayout->addWidget(chkGroup);

    // 积分参数
    auto *paramGroup = new QGroupBox("积分参数", this);
    auto *paramForm = new QFormLayout(paramGroup);
    m_integralType = new QComboBox(paramGroup);
    m_integralType->addItem("斜切");
    m_integralType->addItem("竖切");
    m_integralType->setCurrentText("斜切");

    m_waveWidth = new QDoubleSpinBox(paramGroup);
    m_waveWidth->setRange(0.0, 1000.0);
    m_waveWidth->setDecimals(3);
    m_waveWidth->setValue(1.0);

    m_slopeThreshold = new QDoubleSpinBox(paramGroup);
    m_slopeThreshold->setRange(0.0, 100.0);
    m_slopeThreshold->setDecimals(3);
    m_slopeThreshold->setValue(0.01);

    m_advance = new QDoubleSpinBox(paramGroup);
    m_advance->setRange(-10.0, 10.0);
    m_advance->setDecimals(3);
    m_advance->setValue(0.0);

    m_delay = new QDoubleSpinBox(paramGroup);
    m_delay->setRange(-10.0, 10.0);
    m_delay->setDecimals(3);
    m_delay->setValue(0.0);

    paramForm->addRow("积分类型:", m_integralType);
    paramForm->addRow("波宽:", m_waveWidth);
    paramForm->addRow("斜率:", m_slopeThreshold);
    paramForm->addRow("波形提前:", m_advance);
    paramForm->addRow("波形延后:", m_delay);
    mainLayout->addWidget(paramGroup);

    // 应用按钮
    QPushButton *applyBtn = new QPushButton("应用", this);
    connect(applyBtn, &QPushButton::clicked, this, &IntegralSettingsDialog::applySettings);
    mainLayout->addWidget(applyBtn);
}

void IntegralSettingsDialog::applySettings()
{
    QMap<QString, bool> vis;
    vis["rawA"] = m_chkRawA->isChecked();
    vis["filtA"] = m_chkFiltA->isChecked();
    vis["rawB"] = m_chkRawB->isChecked();
    vis["filtB"] = m_chkFiltB->isChecked();
    vis["rawAB"] = m_chkRawAB->isChecked();
    vis["filtAB"] = m_chkFiltAB->isChecked();

    emit settingsChanged(vis,
                         m_integralType->currentText(),
                         m_waveWidth->value(),
                         m_slopeThreshold->value(),
                         m_advance->value(),
                         m_delay->value());
    hide(); // 应用后隐藏
}

//-------------------------------------------------------------
// IntegrationWorker 实现
//-------------------------------------------------------------
IntegrationWorker::IntegrationWorker(QObject *parent) : QObject(parent)
{
}

void IntegrationWorker::setData(const QVector<double> &time,
                                const QVector<double> &rawA,
                                const QVector<double> &filtA,
                                const QVector<double> &rawB,
                                const QVector<double> &filtB,
                                const QVector<double> &rawAB,
                                const QVector<double> &filtAB)
{
    m_time = time;
    m_rawA = rawA;
    m_filtA = filtA;
    m_rawB = rawB;
    m_filtB = filtB;
    m_rawAB = rawAB;
    m_filtAB = filtAB;
}

void IntegrationWorker::setParameters(double waveWidth, double slopeThreshold,
                                      double advance, double delay,
                                      const QString &integralType,
                                      QMap<QString, bool> visibility)
{
    m_waveWidth = waveWidth;
    m_slopeThreshold = slopeThreshold;
    m_advance = advance;
    m_delay = delay;
    m_integralType = integralType;
    m_visibility = visibility;
}

void IntegrationWorker::processAutoIntegration()
{
    QList<QStringList> results;

    // 根据可见性选择通道
    if (m_visibility.value("rawA", true)) {
        integrateChannel(m_time, m_rawA, "A滤波前", m_integralType, results);
    }
    if (m_visibility.value("filtA", true)) {
        integrateChannel(m_time, m_filtA, "A滤波后", m_integralType, results);
    }
    if (m_visibility.value("rawB", true)) {
        integrateChannel(m_time, m_rawB, "B滤波前", m_integralType, results);
    }
    if (m_visibility.value("filtB", true)) {
        integrateChannel(m_time, m_filtB, "B滤波后", m_integralType, results);
    }
    if (m_visibility.value("rawAB", true)) {
        integrateChannel(m_time, m_rawAB, "A-B滤波前", m_integralType, results);
    }
    if (m_visibility.value("filtAB", true)) {
        integrateChannel(m_time, m_filtAB, "A-B滤波后", m_integralType, results);
    }

    emit integrationFinished(results);
}

void IntegrationWorker::integrateChannel(const QVector<double> &x, const QVector<double> &y,
                                         const QString &channelName, const QString &type,
                                         QList<QStringList> &results)
{
    int n = x.size();
    if (n < 3) return;

    int i = 1;
    int peakCount = 0;
    while (i < n - 1) {
        double dx = x[i] - x[i-1];
        if (qFuzzyIsNull(dx)) { i++; continue; }
        double slope = (y[i] - y[i-1]) / dx;

        if (slope > m_slopeThreshold) {
            // 向前寻找起点
            int startIdx = i - 1;
            while (startIdx > 0) {
                double dxPrev = x[startIdx] - x[startIdx-1];
                if (qFuzzyIsNull(dxPrev)) break;
                double s = (y[startIdx] - y[startIdx-1]) / dxPrev;
                if (s <= m_slopeThreshold) break;
                startIdx--;
            }

            // 寻找顶点
            int peakIdx = startIdx;
            while (peakIdx < n - 1) {
                double dxNext = x[peakIdx+1] - x[peakIdx];
                if (qFuzzyIsNull(dxNext)) break;
                double s = (y[peakIdx+1] - y[peakIdx]) / dxNext;
                if (s <= 0) break;
                peakIdx++;
            }
            double retentionTime = x[peakIdx];

            // 寻找终点
            int endIdx = peakIdx + 1;
            if (endIdx >= n) endIdx = n - 1;
            while (endIdx < n - 1) {
                double dxEnd = x[endIdx] - x[endIdx-1];
                if (qFuzzyIsNull(dxEnd)) break;
                double s = (y[endIdx] - y[endIdx-1]) / dxEnd;
                if (s >= -m_slopeThreshold) break;
                endIdx++;
            }

            // ========== 波宽过滤（新添加） ==========
            double rawPeakWidth = x[endIdx] - x[startIdx];   // 原始峰宽，不含提前/延后
            if (rawPeakWidth < m_waveWidth) {
                i = endIdx + 1;
                continue;   // 跳过窄峰
            }
            // =======================================

            // 应用提前和延后量
            double startX = x[startIdx] - m_advance;
            double endX = x[endIdx] + m_delay;

            // 计算面积（带基线扣除）
            double area = calculateArea(x, y, startIdx, endIdx, type);

            // 生成结果行
            QStringList row;
            row << "自动积分"
                << QString("%1 峰%2").arg(channelName).arg(++peakCount)
                << QString::number(startX, 'f', 3)
                << QString::number(endX, 'f', 3)
                << QString("%1（%2）").arg(channelName).arg(type)
                << QString::number(retentionTime, 'f', 3)
                << QString::number(area, 'f', 3);
            results.append(row);

            i = endIdx + 1;
        } else {
            i++;
        }
    }
}

double IntegrationWorker::calculateArea(const QVector<double> &x, const QVector<double> &y,
                                        int startIdx, int endIdx,
                                        const QString &type)
{
    if (startIdx >= endIdx || endIdx >= x.size()) return 0.0;

    // 统一使用线性基线扣除（无论斜切还是竖切）
    double x0 = x[startIdx];
    double y0 = y[startIdx];
    double x1 = x[endIdx];
    double y1 = y[endIdx];
    double slope = (y1 - y0) / (x1 - x0);

    double area = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i+1] - x[i];
        double baseline1 = y0 + slope * (x[i] - x0);
        double baseline2 = y0 + slope * (x[i+1] - x0);
        double val1 = y[i] - baseline1;
        double val2 = y[i+1] - baseline2;
        area += h * (val1 + val2) / 2.0;
    }

    return area;
}

//-------------------------------------------------------------
// DataProcessingDialog 实现
//-------------------------------------------------------------
DataProcessingDialog::DataProcessingDialog(QWidget *parent)
    : QDialog(parent)
    , m_plot(nullptr)
    , m_zoomGroup(nullptr)
    , m_resultTable(nullptr)
    , m_manualIntegralBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_settingsDialog(nullptr)
    , m_manualIntegralMode(false)
    , m_selectingStart(true)
    , m_startPoint(0)
    , m_startLine(nullptr)
    , m_endLine(nullptr)
    , m_thread(nullptr)
    , m_worker(nullptr)
{
    setWindowTitle("数据处理");
    resize(1200, 800);
    setAttribute(Qt::WA_DeleteOnClose);

    // 初始化可见性
    m_visibility["rawA"] = true;
    m_visibility["filtA"] = true;
    m_visibility["rawB"] = true;
    m_visibility["filtB"] = true;
    m_visibility["rawAB"] = true;
    m_visibility["filtAB"] = true;

    m_integralType = "斜切";
    m_waveWidth = 1.0;
    m_slopeThreshold = 0.01;
    m_advance = 0.0;
    m_delay = 0.0;

    auto *mainLayout = new QVBoxLayout(this);

    // 顶部按钮行
    auto *btnLayout = new QHBoxLayout;
    QPushButton *openBtn = new QPushButton("打开文件", this);
    QPushButton *settingsBtn = new QPushButton("积分设置", this);
    QPushButton *autoIntegrateBtn = new QPushButton("自动积分", this);
    m_manualIntegralBtn = new QPushButton("手动积分", this);
    m_manualIntegralBtn->setCheckable(true);
    QPushButton *exportBtn = new QPushButton("导出", this);
    QPushButton *restoreBtn = new QPushButton("还原图像", this);

    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(settingsBtn);
    btnLayout->addWidget(autoIntegrateBtn);
    btnLayout->addWidget(m_manualIntegralBtn);
    btnLayout->addWidget(exportBtn);
    btnLayout->addWidget(restoreBtn);
    btnLayout->addSpacing(10);

    // 缩放模式按钮
    QRadioButton *zoomX = new QRadioButton("X缩放", this);
    QRadioButton *zoomY = new QRadioButton("Y缩放", this);
    QRadioButton *zoomXY = new QRadioButton("XY缩放", this);
    QRadioButton *zoomRect = new QRadioButton("框选缩放", this);
    zoomXY->setChecked(true);

    m_zoomGroup = new QButtonGroup(this);
    m_zoomGroup->addButton(zoomX, static_cast<int>(InteractivePlot::ZoomX));
    m_zoomGroup->addButton(zoomY, static_cast<int>(InteractivePlot::ZoomY));
    m_zoomGroup->addButton(zoomXY, static_cast<int>(InteractivePlot::ZoomXY));
    m_zoomGroup->addButton(zoomRect, static_cast<int>(InteractivePlot::ZoomRect));

    btnLayout->addWidget(zoomX);
    btnLayout->addWidget(zoomY);
    btnLayout->addWidget(zoomXY);
    btnLayout->addWidget(zoomRect);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    // 状态标签
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: blue; font-weight: bold;");
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    // 绘图区域
    m_plot = new InteractivePlot(this);
    m_plot->xAxis->setLabel("Time(s)");
    m_plot->yAxis->setLabel("Amplitude(mV)");
    setupPlots();
    mainLayout->addWidget(m_plot, 1);

    // 手动积分垂直线
    m_startLine = new QCPItemStraightLine(m_plot);
    m_startLine->setPen(QPen(Qt::darkGreen, 2, Qt::DashLine));
    m_startLine->point1->setCoords(0, 0);
    m_startLine->point2->setCoords(0, 1);
    m_startLine->setVisible(false);

    m_endLine = new QCPItemStraightLine(m_plot);
    m_endLine->setPen(QPen(Qt::red, 2, Qt::DashLine));
    m_endLine->point1->setCoords(0, 0);
    m_endLine->point2->setCoords(0, 1);
    m_endLine->setVisible(false);

    // 结果表格
    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        "文件名", "组份", "起始时间(s)", "结束时间(s)", "类型", "保留时间(s)", "峰面积(s*mV)"
    });
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setMinimumHeight(180);
    mainLayout->addWidget(m_resultTable, 0);

    // 创建积分设置对话框（非模态）
    m_settingsDialog = new IntegralSettingsDialog(this);
    m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose, false); // 手动管理

    // 信号连接
    connect(openBtn, &QPushButton::clicked, this, &DataProcessingDialog::openFile);
    connect(settingsBtn, &QPushButton::clicked, this, &DataProcessingDialog::showIntegralSettings);
    connect(autoIntegrateBtn, &QPushButton::clicked, this, &DataProcessingDialog::autoIntegrate);
    connect(m_manualIntegralBtn, &QPushButton::toggled, this, &DataProcessingDialog::toggleManualIntegral);
    connect(exportBtn, &QPushButton::clicked, this, &DataProcessingDialog::exportData);
    connect(restoreBtn, &QPushButton::clicked, this, &DataProcessingDialog::resetView);
    connect(m_zoomGroup, qOverload<int>(&QButtonGroup::idClicked),
            this, &DataProcessingDialog::applyZoomMode);
    connect(m_plot, &InteractivePlot::rightClicked, this, &DataProcessingDialog::handleRightClick);
    connect(m_settingsDialog, &IntegralSettingsDialog::settingsChanged,
            this, &DataProcessingDialog::updateSettings);

    // 线程初始化
    m_thread = new QThread(this);
    m_worker = new IntegrationWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &DataProcessingDialog::destroyed, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_thread->start();
}

DataProcessingDialog::~DataProcessingDialog()
{
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
}

void DataProcessingDialog::setupPlots()
{
    // 清除旧曲线
    m_plot->clearGraphs();

    // 添加6条曲线并设置名称和颜色
    m_plot->addGraph();
    m_plot->graph(0)->setName("A滤波前");
    m_plot->graph(0)->setPen(QPen(QColor("#0000FF"), 2)); // 深蓝

    m_plot->addGraph();
    m_plot->graph(1)->setName("A滤波后");
    m_plot->graph(1)->setPen(QPen(QColor("#87CEEB"), 2)); // 浅蓝

    m_plot->addGraph();
    m_plot->graph(2)->setName("B滤波前");
    m_plot->graph(2)->setPen(QPen(QColor("#FF0000"), 2)); // 深红

    m_plot->addGraph();
    m_plot->graph(3)->setName("B滤波后");
    m_plot->graph(3)->setPen(QPen(QColor("#FFA07A"), 2)); // 浅红

    m_plot->addGraph();
    m_plot->graph(4)->setName("A-B滤波前");
    m_plot->graph(4)->setPen(QPen(QColor("#008000"), 2)); // 深绿

    m_plot->addGraph();
    m_plot->graph(5)->setName("A-B滤波后");
    m_plot->graph(5)->setPen(QPen(QColor("#90EE90"), 2)); // 浅绿

    // ========== 性能优化 ==========
    m_plot->setPlottingHints(QCP::phFastPolylines);
    m_plot->setNoAntialiasingOnDrag(true);
    // ================================

    // 图例设置（细字体）
    QFont legendFont;
    legendFont.setFamily("Arial");
    legendFont.setPointSize(8);
    legendFont.setWeight(QFont::Light);          // 使用细体字重
    m_plot->legend->setFont(legendFont);
    m_plot->legend->setVisible(true);
    m_plot->legend->setBrush(QBrush(QColor(255,255,255,180))); // 半透明背景
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    m_plot->xAxis->setRange(0, 5);
    m_plot->yAxis->setRange(0, 5);
    m_plot->replot();
}

void DataProcessingDialog::updatePlotVisibility()
{
    m_plot->graph(0)->setVisible(m_visibility.value("rawA", true));
    m_plot->graph(1)->setVisible(m_visibility.value("filtA", true));
    m_plot->graph(2)->setVisible(m_visibility.value("rawB", true));
    m_plot->graph(3)->setVisible(m_visibility.value("filtB", true));
    m_plot->graph(4)->setVisible(m_visibility.value("rawAB", true));
    m_plot->graph(5)->setVisible(m_visibility.value("filtAB", true));
    m_plot->replot();
}

void DataProcessingDialog::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "打开数据文件", QString(),
                                                    "文本文件 (*.txt);;所有文件 (*)");
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件：" + fileName);
        return;
    }

    // 清空缓存
    m_time.clear();
    m_rawA.clear(); m_filtA.clear();
    m_rawB.clear(); m_filtB.clear();
    m_rawAB.clear(); m_filtAB.clear();

    QTextStream in(&file);
    bool firstLine = true;
    int validLines = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        // 跳过表头
        if (firstLine) {
            firstLine = false;
            if (line.contains("时间") || line.contains("Time"))
                continue;
        }

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 7)
            continue;

        // 解析第一列：时间（可以是相对秒数或时分秒格式）
        bool okTime = false;
        double timeSec = parts[0].toDouble(&okTime);
        if (!okTime) {
            // 尝试解析为 HH:mm:ss 或 HH:mm:ss.zzz
            QTime t = QTime::fromString(parts[0], "HH:mm:ss");
            if (!t.isValid()) {
                t = QTime::fromString(parts[0], "HH:mm:ss.zzz");
                if (!t.isValid())
                    continue;
            }
            timeSec = t.hour() * 3600.0 + t.minute() * 60.0 + t.second() + t.msec() / 1000.0;
        }

        // 解析其余6列浮点数
        bool ok = false;
        double rawA = parts[1].toDouble(&ok); if (!ok) continue;
        double filtA = parts[2].toDouble(&ok); if (!ok) continue;
        double rawB = parts[3].toDouble(&ok); if (!ok) continue;
        double filtB = parts[4].toDouble(&ok); if (!ok) continue;
        double rawAB = parts[5].toDouble(&ok); if (!ok) continue;
        double filtAB = parts[6].toDouble(&ok); if (!ok) continue;

        m_time.append(timeSec);
        m_rawA.append(rawA);
        m_filtA.append(filtA);
        m_rawB.append(rawB);
        m_filtB.append(filtB);
        m_rawAB.append(rawAB);
        m_filtAB.append(filtAB);
        validLines++;
    }

    if (validLines == 0) {
        QMessageBox::information(this, "提示", "文件中未找到有效数据。");
        return;
    }

    // 更新曲线
    m_plot->graph(0)->setData(m_time, m_rawA);
    m_plot->graph(1)->setData(m_time, m_filtA);
    m_plot->graph(2)->setData(m_time, m_rawB);
    m_plot->graph(3)->setData(m_time, m_filtB);
    m_plot->graph(4)->setData(m_time, m_rawAB);
    m_plot->graph(5)->setData(m_time, m_filtAB);
    updatePlotVisibility();
    m_plot->rescaleAxes();
    m_plot->replot();
}

void DataProcessingDialog::showIntegralSettings()
{
    if (m_settingsDialog->isHidden()) {
        m_settingsDialog->show();
    } else {
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    }
}

void DataProcessingDialog::toggleManualIntegral(bool active)
{
    m_manualIntegralMode = active;
    if (active) {
        m_selectingStart = true;
        m_startLine->setVisible(false);
        m_endLine->setVisible(false);
        m_plot->replot();
        m_statusLabel->setText("请右键点击曲线选择起点");
        m_statusLabel->setVisible(true);
    } else {
        m_startLine->setVisible(false);
        m_endLine->setVisible(false);
        m_selectingStart = true;
        m_statusLabel->setVisible(false);
        m_plot->replot();
    }
}

double DataProcessingDialog::calculateArea(const QVector<double> &x, const QVector<double> &y,
                                           int startIdx, int endIdx,
                                           const QString &type)
{
    if (startIdx >= endIdx || endIdx >= x.size()) return 0.0;

    // 线性基线扣除（斜切和竖切都使用）
    double x0 = x[startIdx];
    double y0 = y[startIdx];
    double x1 = x[endIdx];
    double y1 = y[endIdx];
    double slope = (y1 - y0) / (x1 - x0);

    double area = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i+1] - x[i];
        double baseline1 = y0 + slope * (x[i] - x0);
        double baseline2 = y0 + slope * (x[i+1] - x0);
        double val1 = y[i] - baseline1;
        double val2 = y[i+1] - baseline2;
        area += h * (val1 + val2) / 2.0;
    }
    return area;
}

void DataProcessingDialog::handleRightClick(const QPointF &scenePos)
{
    if (!m_manualIntegralMode) return;

    if (m_selectingStart) {
        m_startPoint = scenePos.x();
        m_startLine->point1->setCoords(m_startPoint, m_plot->yAxis->range().lower);
        m_startLine->point2->setCoords(m_startPoint, m_plot->yAxis->range().upper);
        m_startLine->setVisible(true);
        m_selectingStart = false;
        m_plot->replot();
        m_statusLabel->setText("请右键点击曲线选择终点");
    } else {
        double endPoint = scenePos.x();
        m_endLine->point1->setCoords(endPoint, m_plot->yAxis->range().lower);
        m_endLine->point2->setCoords(endPoint, m_plot->yAxis->range().upper);
        m_endLine->setVisible(true);
        m_plot->replot();

        double start = qMin(m_startPoint, endPoint);
        double end = qMax(m_startPoint, endPoint);

        // 查找起止索引（基于时间轴）
        int startIdx = -1, endIdx = -1;
        for (int i = 0; i < m_time.size(); ++i) {
            if (m_time[i] >= start && startIdx < 0) startIdx = i;
            if (m_time[i] <= end) endIdx = i;
        }
        if (startIdx < 0 || endIdx < 0 || startIdx >= endIdx) {
            m_statusLabel->setText("选择范围无效");
            return;
        }

        // 遍历所有可见通道，分别积分
        QList<QPair<QString, QVector<double>>> channels;
        if (m_visibility.value("rawA", true)) channels.append({"A滤波前", m_rawA});
        if (m_visibility.value("filtA", true)) channels.append({"A滤波后", m_filtA});
        if (m_visibility.value("rawB", true)) channels.append({"B滤波前", m_rawB});
        if (m_visibility.value("filtB", true)) channels.append({"B滤波后", m_filtB});
        if (m_visibility.value("rawAB", true)) channels.append({"A-B滤波前", m_rawAB});
        if (m_visibility.value("filtAB", true)) channels.append({"A-B滤波后", m_filtAB});

        int peakNum = 1; // 手动积分时，所有通道共用一组峰编号？可根据需要调整
        for (const auto &channel : channels) {
            const QString &name = channel.first;
            const QVector<double> &data = channel.second;
            if (data.size() <= endIdx) continue;

            // 计算面积（带基线扣除）
            double area = calculateArea(m_time, data, startIdx, endIdx, m_integralType);

            addResultToTable("手动积分",
                             QString("%1 峰%2").arg(name).arg(peakNum),
                             start, end,
                             QString("%1（%2）").arg(name).arg(m_integralType),
                             (start + end) / 2.0, area);
            peakNum++;
        }

        // 重置状态，保留垂直线
        m_selectingStart = true;
        m_statusLabel->setText("积分完成，可继续右键选择新峰起点，或再次点击手动积分退出");
    }
}

void DataProcessingDialog::autoIntegrate()
{
    if (m_time.isEmpty()) {
        m_statusLabel->setText("请先打开数据文件");
        m_statusLabel->setVisible(true);
        return;
    }

    // 准备数据并在线程中计算
    m_worker->setData(m_time, m_rawA, m_filtA, m_rawB, m_filtB, m_rawAB, m_filtAB);
    m_worker->setParameters(m_waveWidth, m_slopeThreshold, m_advance, m_delay,
                            m_integralType, m_visibility);

    // 连接信号（如果尚未连接）
    connect(m_worker, &IntegrationWorker::integrationFinished,
            this, &DataProcessingDialog::onIntegrationFinished, Qt::UniqueConnection);

    QMetaObject::invokeMethod(m_worker, "processAutoIntegration", Qt::QueuedConnection);
    m_statusLabel->setText("自动积分计算中...");
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::onIntegrationFinished(const QList<QStringList> &results)
{
    m_resultTable->setRowCount(0); // 清空旧结果
    for (const QStringList &row : results) {
        int r = m_resultTable->rowCount();
        m_resultTable->insertRow(r);
        for (int c = 0; c < row.size() && c < 7; ++c) {
            m_resultTable->setItem(r, c, new QTableWidgetItem(row[c]));
        }
    }
    m_statusLabel->setText("自动积分完成，结果已更新。");
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::updateSettings(QMap<QString, bool> visibility,
                                          QString integralType,
                                          double waveWidth,
                                          double slopeThreshold,
                                          double advance,
                                          double delay)
{
    m_visibility = visibility;
    m_integralType = integralType;
    m_waveWidth = waveWidth;
    m_slopeThreshold = slopeThreshold;
    m_advance = advance;
    m_delay = delay;

    updatePlotVisibility();
}

void DataProcessingDialog::exportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出结果", QString(), "CSV 文件 (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法写入文件：" + fileName);
        return;
    }

    QTextStream out(&file);
    for (int col = 0; col < m_resultTable->columnCount(); ++col) {
        out << m_resultTable->horizontalHeaderItem(col)->text();
        if (col < m_resultTable->columnCount() - 1) out << ",";
    }
    out << "\n";
    for (int row = 0; row < m_resultTable->rowCount(); ++row) {
        for (int col = 0; col < m_resultTable->columnCount(); ++col) {
            QTableWidgetItem *item = m_resultTable->item(row, col);
            out << (item ? item->text() : QString());
            if (col < m_resultTable->columnCount() - 1) out << ",";
        }
        out << "\n";
    }
    file.close();
    QMessageBox::information(this, "导出", "计算结果已导出到：" + fileName);
}

void DataProcessingDialog::resetView()
{
    m_plot->rescaleAxes();
    m_plot->replot();
}

void DataProcessingDialog::applyZoomMode(int id)
{
    auto mode = static_cast<InteractivePlot::ZoomMode>(id);
    m_plot->setZoomMode(mode);
}

void DataProcessingDialog::addResultToTable(const QString &fileName, const QString &component,
                                            double start, double end, const QString &type,
                                            double retention, double area)
{
    int row = m_resultTable->rowCount();
    m_resultTable->insertRow(row);
    m_resultTable->setItem(row, 0, new QTableWidgetItem(fileName));
    m_resultTable->setItem(row, 1, new QTableWidgetItem(component));
    m_resultTable->setItem(row, 2, new QTableWidgetItem(QString::number(start, 'f', 3)));
    m_resultTable->setItem(row, 3, new QTableWidgetItem(QString::number(end, 'f', 3)));
    m_resultTable->setItem(row, 4, new QTableWidgetItem(type));
    m_resultTable->setItem(row, 5, new QTableWidgetItem(QString::number(retention, 'f', 3)));
    m_resultTable->setItem(row, 6, new QTableWidgetItem(QString::number(area, 'f', 3)));
}