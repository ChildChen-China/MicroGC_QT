#include "dataprocessing.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
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
#include <QtMath>
#include <QDebug>
#include <QApplication>
#include <QElapsedTimer>
#include <QSettings>

#include "interactiveplot.h"
#include "qcustomplot.h"

//==========================================================
// IntegralSettingsDialog 实现
//==========================================================
IntegralSettingsDialog::IntegralSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("积分设置");
    setMinimumWidth(350);

    auto *mainLayout = new QVBoxLayout(this);

    auto *chkGroup = new QGroupBox("显示通道", this);
    auto *chkLayout = new QGridLayout(chkGroup);
    m_chkRawA = new QCheckBox("A滤波前", chkGroup);
    m_chkFiltA = new QCheckBox("A滤波后", chkGroup);
    m_chkRawB = new QCheckBox("B滤波前", chkGroup);
    m_chkFiltB = new QCheckBox("B滤波后", chkGroup);
    m_chkRawAB = new QCheckBox("A-B滤波前", chkGroup);
    m_chkFiltAB = new QCheckBox("A-B滤波后", chkGroup);

    chkLayout->addWidget(m_chkRawA, 0, 0); chkLayout->addWidget(m_chkFiltA, 0, 1);
    chkLayout->addWidget(m_chkRawB, 1, 0); chkLayout->addWidget(m_chkFiltB, 1, 1);
    chkLayout->addWidget(m_chkRawAB, 2, 0); chkLayout->addWidget(m_chkFiltAB, 2, 1);
    mainLayout->addWidget(chkGroup);

    auto *paramGroup = new QGroupBox("积分参数", this);
    auto *paramForm = new QFormLayout(paramGroup);
    m_integralType = new QComboBox(paramGroup);
    m_integralType->addItem("斜切");
    m_integralType->addItem("竖切");

    m_waveWidth = new QDoubleSpinBox(paramGroup);
    m_waveWidth->setRange(0.0, 1000.0);
    m_waveWidth->setDecimals(3);
    m_waveWidth->setToolTip("最小峰宽，单位：秒");

    m_slopeThreshold = new QDoubleSpinBox(paramGroup);
    m_slopeThreshold->setRange(0.0, 1000000.0);
    m_slopeThreshold->setDecimals(3);
    m_slopeThreshold->setToolTip("斜率阈值，单位：uV/s。需大于噪声斜率");

    m_advance = new QDoubleSpinBox(paramGroup);
    m_advance->setRange(-100.0, 100.0); m_advance->setDecimals(3);
    m_advance->setToolTip("波形提前(秒)：正数=起点向前延伸，负数=向后收缩");

    m_delay = new QDoubleSpinBox(paramGroup);
    m_delay->setRange(-100.0, 100.0); m_delay->setDecimals(3);
    m_delay->setToolTip("波形延后(秒)：正数=终点向后延伸，负数=向前收缩");

    m_chkRemoveMean = new QCheckBox("去均值(消除直流偏置)", paramGroup);

    paramForm->addRow("积分类型:", m_integralType);
    paramForm->addRow("波宽(s):", m_waveWidth);
    paramForm->addRow("斜率:", m_slopeThreshold);
    paramForm->addRow("波形提前:", m_advance);
    paramForm->addRow("波形延后:", m_delay);
    paramForm->addRow("数据处理:", m_chkRemoveMean);
    mainLayout->addWidget(paramGroup);

    QPushButton *applyBtn = new QPushButton("应用", this);
    connect(applyBtn, &QPushButton::clicked, this, &IntegralSettingsDialog::applySettings);
    mainLayout->addWidget(applyBtn);

    // [QSettings] 构造时加载上次保存的值到控件
    {
        QSettings s("MyCompany", "MicroGC");

        m_chkRawA->setChecked(s.value("dp/vis_rawA", true).toBool());
        m_chkFiltA->setChecked(s.value("dp/vis_filtA", true).toBool());
        m_chkRawB->setChecked(s.value("dp/vis_rawB", true).toBool());
        m_chkFiltB->setChecked(s.value("dp/vis_filtB", true).toBool());
        m_chkRawAB->setChecked(s.value("dp/vis_rawAB", true).toBool());
        m_chkFiltAB->setChecked(s.value("dp/vis_filtAB", true).toBool());

        QString typeStr = s.value("dp/integralType", "斜切").toString();
        int idx = m_integralType->findText(typeStr);
        m_integralType->setCurrentIndex(idx >= 0 ? idx : 0);

        m_waveWidth->setValue(s.value("dp/waveWidth", 0.6).toDouble());
        m_slopeThreshold->setValue(s.value("dp/slopeThreshold", 50.0).toDouble());
        m_advance->setValue(s.value("dp/advance", 0.0).toDouble());
        m_delay->setValue(s.value("dp/delay", 0.0).toDouble());
        m_chkRemoveMean->setChecked(s.value("dp/removeMean", false).toBool());
    }
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

    // [QSettings] 应用时把当前值写回
    {
        QSettings s("MyCompany", "MicroGC");

        s.setValue("dp/vis_rawA",  m_chkRawA->isChecked());
        s.setValue("dp/vis_filtA", m_chkFiltA->isChecked());
        s.setValue("dp/vis_rawB",  m_chkRawB->isChecked());
        s.setValue("dp/vis_filtB", m_chkFiltB->isChecked());
        s.setValue("dp/vis_rawAB", m_chkRawAB->isChecked());
        s.setValue("dp/vis_filtAB",m_chkFiltAB->isChecked());

        s.setValue("dp/integralType",    m_integralType->currentText());
        s.setValue("dp/waveWidth",       m_waveWidth->value());
        s.setValue("dp/slopeThreshold",  m_slopeThreshold->value());
        s.setValue("dp/advance",         m_advance->value());
        s.setValue("dp/delay",           m_delay->value());
        s.setValue("dp/removeMean",      m_chkRemoveMean->isChecked());

        s.sync();
    }

    emit settingsChanged(vis, m_integralType->currentText(),
                         m_waveWidth->value(), m_slopeThreshold->value(),
                         m_advance->value(), m_delay->value(),
                         m_chkRemoveMean->isChecked());
    hide();
}

//==========================================================
// IntegrationWorker 实现
//==========================================================
IntegrationWorker::IntegrationWorker(QObject *parent) : QObject(parent) {}

void IntegrationWorker::setData(const QVector<double> &time,
                                const QVector<double> &rawA, const QVector<double> &filtA,
                                const QVector<double> &rawB, const QVector<double> &filtB,
                                const QVector<double> &rawAB, const QVector<double> &filtAB)
{
    m_time = time;
    m_rawA = rawA; m_filtA = filtA;
    m_rawB = rawB; m_filtB = filtB;
    m_rawAB = rawAB; m_filtAB = filtAB;
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
    m_warnings.clear();

    if (m_visibility.value("rawA", true))    integrateChannel(m_time, m_rawA,   "A滤波前",   m_integralType, results);
    if (m_visibility.value("filtA", true))   integrateChannel(m_time, m_filtA,  "A滤波后",   m_integralType, results);
    if (m_visibility.value("rawB", true))    integrateChannel(m_time, m_rawB,   "B滤波前",   m_integralType, results);
    if (m_visibility.value("filtB", true))   integrateChannel(m_time, m_filtB,  "B滤波后",   m_integralType, results);
    if (m_visibility.value("rawAB", true))   integrateChannel(m_time, m_rawAB,  "A-B滤波前", m_integralType, results);
    if (m_visibility.value("filtAB", true))  integrateChannel(m_time, m_filtAB, "A-B滤波后", m_integralType, results);

    emit integrationFinished(results);
    if (!m_warnings.isEmpty()) emit warningsGenerated(m_warnings);
}

// ==========================================================
// 峰检测核心 —— 修复 1（自适应回溯）+ 修复 3（局部窗口阈值）
//                + advance/delay 真正作用于面积区间
// ==========================================================
void IntegrationWorker::integrateChannel(const QVector<double> &x, const QVector<double> &y,
                                         const QString &channelName, const QString &type,
                                         QList<QStringList> &results)
{
    int n = x.size();
    if (n < 10) return;

    double dt = 0.0;
    if (n > 1) dt = (x[n - 1] - x[0]) / (n - 1);

    const double riseThreshold = m_slopeThreshold;
    const double fallThreshold = m_slopeThreshold;
    const double flatThreshold = m_slopeThreshold * 0.3;
    const int    confirmPoints = 3;

    const int maxSearchBack    = 5000;
    const int maxSearchForward = 5000;

    auto slopeAt = [&](int i) -> double {
        int j = i + 1;
        while (j < n && qAbs(x[j] - x[i]) < 1e-12) j++;
        if (j >= n) return 0.0;
        double dx = x[j] - x[i];
        if (qFuzzyIsNull(dx)) return 0.0;
        return (y[j] - y[i]) / dx;
    };

    auto smoothedSlope = [&](int i) -> double {
        int a = qMax(0, i - 2);
        int b = qMin(n - 1, i + 2);
        if (qAbs(x[b] - x[a]) < 1e-12) return slopeAt(i);
        if (qIsNaN(y[a]) || qIsNaN(y[b])) return slopeAt(i);
        return (y[b] - y[a]) / (x[b] - x[a]);
    };

    auto localDataRange = [&](int lo, int hi) -> double {
        lo = qMax(0, lo);
        hi = qMin(n - 1, hi);
        double yMin = 1e30, yMax = -1e30;
        for (int k = lo; k <= hi; ++k) {
            if (qIsNaN(y[k])) continue;
            if (y[k] < yMin) yMin = y[k];
            if (y[k] > yMax) yMax = y[k];
        }
        return (yMin > yMax) ? 0.0 : (yMax - yMin);
    };

    // 时间反查索引（二分，兼容不均匀采样）
    auto findIdxGE = [&](double target) -> int {
        int lo = 0, hi = n - 1, ans = n - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (x[mid] >= target) { ans = mid; hi = mid - 1; }
            else lo = mid + 1;
        }
        return ans;
    };
    auto findIdxLE = [&](double target) -> int {
        int lo = 0, hi = n - 1, ans = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (x[mid] <= target) { ans = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        return ans;
    };

    int ptsByWave = 20;
    if (dt > 1e-12) ptsByWave = qMax(5, (int)(m_waveWidth / dt));
    int localWindowHalf = qBound(200, ptsByWave * 5, 1000);

    int i = 1;
    int peakCount = 0;
    int iterations = 0;
    const int maxIterations = n * 3 + 100;

    while (i < n - 1 && iterations++ < maxIterations)
    {
        if (qIsNaN(y[i]) || qIsNaN(y[i - 1])) { i++; continue; }

        // 步骤 1：SS
        bool isSS = true;
        for (int k = 0; k < confirmPoints; ++k) {
            if (i + k >= n - 1) { isSS = false; break; }
            if (slopeAt(i + k) <= riseThreshold) { isSS = false; break; }
        }
        if (!isSS) { i++; continue; }
        int ssIdx = i;

        // 步骤 2：跟踪峰顶
        double tempRange = localDataRange(qMax(0, ssIdx - 200), qMin(n - 1, ssIdx + 200));
        double tempDrop = qMax(0.5, tempRange * 0.05);

        int peakIdx = ssIdx;
        double localMaxY = y[ssIdx];
        for (int k = ssIdx + 1; k < n; ++k) {
            if (qIsNaN(y[k])) break;
            if (y[k] >= localMaxY) { localMaxY = y[k]; peakIdx = k; }
            else if (localMaxY - y[k] >= tempDrop) break;
        }

        // 修复 3：局部窗口阈值
        double localRange = localDataRange(peakIdx - localWindowHalf,
                                           peakIdx + localWindowHalf);
        double minPeakHeight = qMax(0.5, localRange * 0.03);

        double peakHeight = localMaxY - y[ssIdx];
        if (peakHeight < minPeakHeight) {
            i = qMax(i + 1, peakIdx + 1);
            continue;
        }

        // 修复 1：自适应回溯起点
        int startIdx = ssIdx;
        bool foundS = false;
        int backLimit = qMin(ssIdx, maxSearchBack);
        for (int k = ssIdx - 1; k >= ssIdx - backLimit; --k) {
            if (qAbs(smoothedSlope(k)) < flatThreshold) {
                startIdx = k; foundS = true; break;
            }
        }
        if (!foundS) {
            double minY = y[ssIdx];
            for (int k = ssIdx - 1; k >= ssIdx - backLimit; --k) {
                if (qIsNaN(y[k])) break;
                if (y[k] < minY) { minY = y[k]; startIdx = k; }
            }
        }

        // 步骤 4：ES
        int esIdx = -1;
        for (int k = peakIdx + 1; k < n - confirmPoints; ++k) {
            bool isES = true;
            for (int m = 0; m < confirmPoints; ++m) {
                if (slopeAt(k + m) >= -fallThreshold) { isES = false; break; }
            }
            if (isES) { esIdx = k; break; }
        }
        if (esIdx < 0) { i = qMax(i + 1, peakIdx + 1); continue; }

        // 修复 1：自适应前向搜索终点
        int endIdx = esIdx;
        bool foundE = false;
        int forwardLimit = qMin(n - 1 - esIdx, maxSearchForward);
        for (int k = esIdx + 1; k <= esIdx + forwardLimit; ++k) {
            if (qAbs(smoothedSlope(k)) < flatThreshold) {
                endIdx = k; foundE = true; break;
            }
        }
        if (!foundE) endIdx = qMin(n - 1, esIdx + forwardLimit);

        // 波宽过滤（用原始识别出的峰宽，不含 advance/delay）
        double rawPeakWidth = x[endIdx] - x[startIdx];
        if (rawPeakWidth < m_waveWidth) {
            i = qMax(i + 1, endIdx + 1);
            continue;
        }

        // ==========================================================
        // ★ 修复：把 advance / delay 真正作用到积分区间
        // ==========================================================
        double rawStartTime = x[startIdx];
        double rawEndTime   = x[endIdx];
        double adjStartTime = rawStartTime - m_advance;   // 正=起点向前延伸
        double adjEndTime   = rawEndTime   + m_delay;     // 正=终点向后延伸

        int adjStartIdx = qBound(0, findIdxGE(adjStartTime), n - 1);
        int adjEndIdx   = qBound(0, findIdxLE(adjEndTime),   n - 1);

        if (adjStartIdx >= adjEndIdx) {
            // 扩展后区间非法（比如 advance 太大把区间反转）
            i = qMax(i + 1, endIdx + 1);
            continue;
        }

        // 用扩展后的索引做面积积分
        double area = calculateArea(x, y, adjStartIdx, adjEndIdx, type);

        // 表格显示用扩展后的实际时间
        double retentionTime = x[peakIdx];
        double shownStart    = x[adjStartIdx];
        double shownEnd      = x[adjEndIdx];

        QStringList row;
        row << "自动积分"
            << QString("%1 峰%2").arg(channelName).arg(++peakCount)
            << QString::number(shownStart, 'f', 3)
            << QString::number(shownEnd,   'f', 3)
            << QString("%1（%2）").arg(channelName).arg(type)
            << QString::number(retentionTime, 'f', 3)
            << QString::number(area, 'f', 3);
        results.append(row);

        i = qMax(i + 1, endIdx + 1);
    }
}

double IntegrationWorker::calculateArea(const QVector<double> &x, const QVector<double> &y,
                                        int startIdx, int endIdx,
                                        const QString &type)
{
    if (startIdx < 0 || endIdx < 0) return 0.0;
    if (startIdx >= endIdx || endIdx >= x.size()) return 0.0;
    for (int i = startIdx; i <= endIdx; ++i) if (qIsNaN(y[i])) return 0.0;

    double x0 = x[startIdx], y0 = y[startIdx];
    double x1 = x[endIdx], y1 = y[endIdx];
    double slope = 0.0, baselineY = 0.0;
    if (type.contains("竖切")) { slope = 0.0; baselineY = qMin(y0, y1); }
    else {
        if (qAbs(x1 - x0) < 1e-12) slope = 0.0;
        else slope = (y1 - y0) / (x1 - x0);
        baselineY = y0;
    }

    double areaRaw = 0.0, areaAbs = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i + 1] - x[i];
        if (qFuzzyIsNull(h)) continue;
        double baseline1, baseline2;
        if (type.contains("竖切")) { baseline1 = baselineY; baseline2 = baselineY; }
        else {
            baseline1 = baselineY + slope * (x[i]     - x0);
            baseline2 = baselineY + slope * (x[i + 1] - x0);
        }
        double val1 = y[i]     - baseline1;
        double val2 = y[i + 1] - baseline2;
        areaRaw += h * (val1 + val2) / 2.0;
        areaAbs += h * (qAbs(val1) + qAbs(val2)) / 2.0;
    }
    if (areaRaw < 0) return areaAbs;

    if (areaAbs > 1e-9) {
        double diffRatio = qAbs(areaRaw - areaAbs) / areaAbs;
        if (diffRatio > 0.1) {
            m_warnings.append(QString("峰[%1, %2] 原始面积=%3 绝对面积=%4 差异过大，基线可能不准")
                                  .arg(x0, 0, 'f', 3).arg(x1, 0, 'f', 3)
                                  .arg(areaRaw, 0, 'f', 3).arg(areaAbs, 0, 'f', 3));
        }
    } else if (qAbs(areaRaw) > 1e-9) {
        m_warnings.append(QString("峰[%1, %2] 原始面积=%3 绝对面积=0，基线完全错误")
                              .arg(x0, 0, 'f', 3).arg(x1, 0, 'f', 3).arg(areaRaw, 0, 'f', 3));
    }
    return areaAbs;
}

//==========================================================
// DataProcessingDialog 实现
//==========================================================
DataProcessingDialog::DataProcessingDialog(QWidget *parent)
    : QDialog(parent)
    , m_plot(nullptr), m_zoomGroup(nullptr), m_resultTable(nullptr)
    , m_manualIntegralBtn(nullptr), m_statusLabel(nullptr)
    , m_settingsDialog(nullptr), m_manualIntegralMode(false)
    , m_selectingStart(true), m_startPoint(0)
    , m_startLine(nullptr), m_endLine(nullptr)
    , m_removeMean(false)
{
    setWindowTitle("数据处理");
    resize(1200, 800);
    setAttribute(Qt::WA_DeleteOnClose);

    // [QSettings] 加载上次保存的积分参数
    {
        QSettings s("MyCompany", "MicroGC");

        m_visibility["rawA"]  = s.value("dp/vis_rawA",  true).toBool();
        m_visibility["filtA"] = s.value("dp/vis_filtA", true).toBool();
        m_visibility["rawB"]  = s.value("dp/vis_rawB",  true).toBool();
        m_visibility["filtB"] = s.value("dp/vis_filtB", true).toBool();
        m_visibility["rawAB"] = s.value("dp/vis_rawAB", true).toBool();
        m_visibility["filtAB"]= s.value("dp/vis_filtAB",true).toBool();

        m_integralType   = s.value("dp/integralType",   "斜切").toString();
        m_waveWidth      = s.value("dp/waveWidth",      0.6).toDouble();
        m_slopeThreshold = s.value("dp/slopeThreshold", 50.0).toDouble();
        m_advance        = s.value("dp/advance",        0.0).toDouble();
        m_delay          = s.value("dp/delay",          0.0).toDouble();
        m_removeMean     = s.value("dp/removeMean",     false).toBool();
    }

    auto *mainLayout = new QVBoxLayout(this);

    auto *btnLayout = new QHBoxLayout;
    QPushButton *openBtn = new QPushButton("打开文件", this);
    QPushButton *settingsBtn = new QPushButton("积分设置", this);
    QPushButton *autoIntegrateBtn = new QPushButton("自动积分", this);
    m_manualIntegralBtn = new QPushButton("手动积分", this);
    m_manualIntegralBtn->setCheckable(true);
    QPushButton *exportBtn = new QPushButton("导出", this);
    QPushButton *restoreBtn = new QPushButton("还原图像", this);

    btnLayout->addWidget(openBtn); btnLayout->addWidget(settingsBtn);
    btnLayout->addWidget(autoIntegrateBtn); btnLayout->addWidget(m_manualIntegralBtn);
    btnLayout->addWidget(exportBtn); btnLayout->addWidget(restoreBtn);
    btnLayout->addSpacing(10);

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
    btnLayout->addWidget(zoomX); btnLayout->addWidget(zoomY);
    btnLayout->addWidget(zoomXY); btnLayout->addWidget(zoomRect);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: blue; font-weight: bold;");
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    m_plot = new InteractivePlot(this);
    m_plot->xAxis->setLabel("Time(s)");
    m_plot->yAxis->setLabel("Signal(uV)");
    setupPlots();
    mainLayout->addWidget(m_plot, 1);

    m_startLine = new QCPItemStraightLine(m_plot);
    m_startLine->setPen(QPen(Qt::darkGreen, 2, Qt::DashLine));
    m_startLine->point1->setCoords(0, 0); m_startLine->point2->setCoords(0, 1);
    m_startLine->setVisible(false);

    m_endLine = new QCPItemStraightLine(m_plot);
    m_endLine->setPen(QPen(Qt::red, 2, Qt::DashLine));
    m_endLine->point1->setCoords(0, 0); m_endLine->point2->setCoords(0, 1);
    m_endLine->setVisible(false);

    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(7);
    m_resultTable->setHorizontalHeaderLabels({
        "文件名", "组份", "起始时间(s)", "结束时间(s)", "类型", "保留时间(s)", "峰面积(s*uV)"
    });
    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setMinimumHeight(180);
    mainLayout->addWidget(m_resultTable, 0);

    m_settingsDialog = new IntegralSettingsDialog(this);
    m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);

    m_worker = new IntegrationWorker(this);

    connect(openBtn, &QPushButton::clicked, this, &DataProcessingDialog::openFile);
    connect(settingsBtn, &QPushButton::clicked, this, &DataProcessingDialog::showIntegralSettings);
    connect(autoIntegrateBtn, &QPushButton::clicked, this, &DataProcessingDialog::autoIntegrate);
    connect(m_manualIntegralBtn, &QPushButton::toggled, this, &DataProcessingDialog::toggleManualIntegral);
    connect(exportBtn, &QPushButton::clicked, this, &DataProcessingDialog::exportData);
    connect(restoreBtn, &QPushButton::clicked, this, &DataProcessingDialog::resetView);
    connect(m_zoomGroup, qOverload<int>(&QButtonGroup::idClicked), this, &DataProcessingDialog::applyZoomMode);
    connect(m_plot, &InteractivePlot::rightClicked, this, &DataProcessingDialog::handleRightClick);
    connect(m_settingsDialog, &IntegralSettingsDialog::settingsChanged, this, &DataProcessingDialog::updateSettings);

    connect(m_worker, &IntegrationWorker::integrationFinished, this, &DataProcessingDialog::onIntegrationFinished);
    connect(m_worker, &IntegrationWorker::warningsGenerated, this, [this](const QList<QString> &warnings) {
        if (warnings.isEmpty()) return;
        QString text = QString("自动积分过程中发现 %1 处基线异常：\n\n").arg(warnings.size());
        int showCount = qMin(10, warnings.size());
        for (int i = 0; i < showCount; ++i) text += warnings.at(i) + "\n";
        if (warnings.size() > showCount)
            text += QString("\n...（共 %1 条，仅显示前 %2 条）").arg(warnings.size()).arg(showCount);
        QMessageBox::warning(this, "积分警告", text);
    });
}

DataProcessingDialog::~DataProcessingDialog() {}

void DataProcessingDialog::setupPlots()
{
    m_plot->clearGraphs();
    m_plot->addGraph(); m_plot->graph(0)->setName("A滤波前"); m_plot->graph(0)->setPen(QPen(QColor("#0000FF"), 1));
    m_plot->addGraph(); m_plot->graph(1)->setName("A滤波后"); m_plot->graph(1)->setPen(QPen(QColor("#87CEEB"), 1));
    m_plot->addGraph(); m_plot->graph(2)->setName("B滤波前"); m_plot->graph(2)->setPen(QPen(QColor("#FF0000"), 1));
    m_plot->addGraph(); m_plot->graph(3)->setName("B滤波后"); m_plot->graph(3)->setPen(QPen(QColor("#FFA07A"), 1));
    m_plot->addGraph(); m_plot->graph(4)->setName("A-B滤波前"); m_plot->graph(4)->setPen(QPen(QColor("#008000"), 1));
    m_plot->addGraph(); m_plot->graph(5)->setName("A-B滤波后"); m_plot->graph(5)->setPen(QPen(QColor("#90EE90"), 1));

    m_plot->setPlottingHints(QCP::phFastPolylines);
    m_plot->setNoAntialiasingOnDrag(true);

    QFont legendFont; legendFont.setFamily("Arial"); legendFont.setPointSize(8); legendFont.setWeight(QFont::Light);
    m_plot->legend->setFont(legendFont);
    m_plot->legend->setVisible(true);
    m_plot->legend->setBrush(QBrush(QColor(255,255,255,180)));
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

void DataProcessingDialog::applyDataProcessing()
{
    if (m_timeOrig.isEmpty()) return;
    m_time = m_timeOrig;

    if (!m_removeMean) {
        m_rawA = m_rawAOrig; m_filtA = m_filtAOrig;
        m_rawB = m_rawBOrig; m_filtB = m_filtBOrig;
        m_rawAB = m_rawABOrig; m_filtAB = m_filtABOrig;
    } else {
        auto calcMean = [](const QVector<double>& data) {
            if (data.isEmpty()) return 0.0;
            double sum = 0;
            for (double v : data) sum += v;
            return sum / data.size();
        };
        double meanA = calcMean(m_rawAOrig);
        double meanB = calcMean(m_rawBOrig);
        double meanAB = calcMean(m_rawABOrig);

        int n = m_rawAOrig.size();
        m_rawA.resize(n);   m_filtA.resize(n);
        m_rawB.resize(n);   m_filtB.resize(n);
        m_rawAB.resize(n);  m_filtAB.resize(n);

        for (int i = 0; i < n; ++i) {
            m_rawA[i]   = m_rawAOrig[i]  - meanA;
            m_filtA[i]  = m_filtAOrig[i] - meanA;
            m_rawB[i]   = m_rawBOrig[i]  - meanB;
            m_filtB[i]  = m_filtBOrig[i] - meanB;
            m_rawAB[i]  = m_rawABOrig[i] - meanAB;
            m_filtAB[i] = m_filtABOrig[i]- meanAB;
        }
    }

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

void DataProcessingDialog::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "打开数据文件", QString(), "文本文件 (*.txt);;所有文件 (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件：" + fileName);
        return;
    }

    m_timeOrig.clear();
    m_rawAOrig.clear(); m_filtAOrig.clear();
    m_rawBOrig.clear(); m_filtBOrig.clear();
    m_rawABOrig.clear(); m_filtABOrig.clear();

    QTextStream in(&file);
    bool firstLine = true;
    int validLines = 0;

    auto parseValue = [](const QString &str, bool &ok) -> double {
        if (str.compare("nan", Qt::CaseInsensitive) == 0) { ok = true; return qQNaN(); }
        return str.toDouble(&ok);
    };

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        if (firstLine) {
            firstLine = false;
            if (line.contains("时间") || line.contains("Time")) continue;
        }

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 7) continue;

        bool okTime = false;
        double timeSec = parts[0].toDouble(&okTime);
        if (!okTime) {
            QTime t = QTime::fromString(parts[0], "HH:mm:ss");
            if (!t.isValid()) {
                t = QTime::fromString(parts[0], "HH:mm:ss.zzz");
                if (!t.isValid()) continue;
            }
            timeSec = t.hour() * 3600.0 + t.minute() * 60.0 + t.second() + t.msec() / 1000.0;
        }

        bool ok = false;
        double rawA = parseValue(parts[1], ok); if (!ok) continue;
        double filtA = parseValue(parts[2], ok); if (!ok) continue;
        double rawB = parseValue(parts[3], ok); if (!ok) continue;
        double filtB = parseValue(parts[4], ok); if (!ok) continue;
        double rawAB = parseValue(parts[5], ok); if (!ok) continue;
        double filtAB = parseValue(parts[6], ok); if (!ok) continue;

        m_timeOrig.append(timeSec);
        m_rawAOrig.append(rawA); m_filtAOrig.append(filtA);
        m_rawBOrig.append(rawB); m_filtBOrig.append(filtB);
        m_rawABOrig.append(rawAB); m_filtABOrig.append(filtAB);
        validLines++;
    }

    if (validLines == 0) {
        QMessageBox::information(this, "提示", "文件中未找到有效数据。");
        return;
    }

    {
        QSharedPointer<QCPAxisTicker> xTicker(new QCPAxisTicker);
        xTicker->setTickCount(10);
        xTicker->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
        m_plot->xAxis->setTicker(xTicker);
        m_plot->xAxis->setNumberFormat("f");
        m_plot->xAxis->setNumberPrecision(2);
    }
    {
        QSharedPointer<QCPAxisTicker> yTicker(new QCPAxisTicker);
        yTicker->setTickCount(8);
        yTicker->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
        m_plot->yAxis->setTicker(yTicker);
        m_plot->yAxis->setNumberFormat("f");
        m_plot->yAxis->setNumberPrecision(1);
    }

    applyDataProcessing();
}

void DataProcessingDialog::showIntegralSettings()
{
    if (m_settingsDialog->isHidden()) m_settingsDialog->show();
    else { m_settingsDialog->raise(); m_settingsDialog->activateWindow(); }
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
                                           int startIdx, int endIdx, const QString &type)
{
    if (startIdx < 0 || endIdx < 0) return 0.0;
    if (startIdx >= endIdx || endIdx >= x.size()) return 0.0;
    for (int i = startIdx; i <= endIdx; ++i) if (qIsNaN(y[i])) return 0.0;

    double x0 = x[startIdx], y0 = y[startIdx];
    double x1 = x[endIdx], y1 = y[endIdx];
    double slope = 0.0, baselineY = 0.0;
    if (type.contains("竖切")) { slope = 0.0; baselineY = qMin(y0, y1); }
    else {
        if (qAbs(x1 - x0) < 1e-12) slope = 0.0;
        else slope = (y1 - y0) / (x1 - x0);
        baselineY = y0;
    }

    double areaRaw = 0.0, areaAbs = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i + 1] - x[i];
        if (qFuzzyIsNull(h)) continue;
        double baseline1, baseline2;
        if (type.contains("竖切")) { baseline1 = baselineY; baseline2 = baselineY; }
        else {
            baseline1 = baselineY + slope * (x[i]     - x0);
            baseline2 = baselineY + slope * (x[i + 1] - x0);
        }
        double val1 = y[i]     - baseline1;
        double val2 = y[i + 1] - baseline2;
        areaRaw += h * (val1 + val2) / 2.0;
        areaAbs += h * (qAbs(val1) + qAbs(val2)) / 2.0;
    }
    if (areaRaw < 0) return areaAbs;

    if (areaAbs > 1e-9) {
        double diffRatio = qAbs(areaRaw - areaAbs) / areaAbs;
        if (diffRatio > 0.1) {
            m_manualWarnings.append(QString("峰[%1, %2] 原始面积=%3 绝对面积=%4 差异过大，基线可能不准")
                                        .arg(x0, 0, 'f', 3).arg(x1, 0, 'f', 3)
                                        .arg(areaRaw, 0, 'f', 3).arg(areaAbs, 0, 'f', 3));
        }
    } else if (qAbs(areaRaw) > 1e-9) {
        m_manualWarnings.append(QString("峰[%1, %2] 原始面积=%3 绝对面积=0，基线完全错误")
                                    .arg(x0, 0, 'f', 3).arg(x1, 0, 'f', 3).arg(areaRaw, 0, 'f', 3));
    }
    return areaAbs;
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

        int startIdx = -1, endIdx = -1;
        for (int i = 0; i < m_time.size(); ++i) {
            if (m_time[i] >= start && startIdx < 0) startIdx = i;
            if (m_time[i] <= end) endIdx = i;
        }
        if (startIdx < 0 || endIdx < 0 || startIdx >= endIdx) {
            m_statusLabel->setText("选择范围无效"); return;
        }

        QList<QPair<QString, QVector<double>>> channels;
        if (m_visibility.value("rawA", true))   channels.append({"A滤波前", m_rawA});
        if (m_visibility.value("filtA", true))  channels.append({"A滤波后", m_filtA});
        if (m_visibility.value("rawB", true))   channels.append({"B滤波前", m_rawB});
        if (m_visibility.value("filtB", true))  channels.append({"B滤波后", m_filtB});
        if (m_visibility.value("rawAB", true))  channels.append({"A-B滤波前", m_rawAB});
        if (m_visibility.value("filtAB", true)) channels.append({"A-B滤波后", m_filtAB});

        m_manualWarnings.clear();
        int peakNum = 1;
        for (const auto &channel : channels) {
            const QString &name = channel.first;
            const QVector<double> &data = channel.second;
            if (data.size() <= endIdx) continue;

            double area = calculateArea(m_time, data, startIdx, endIdx, m_integralType);
            addResultToTable("手动积分",
                             QString("%1 峰%2").arg(name).arg(peakNum),
                             start, end,
                             QString("%1（%2）").arg(name).arg(m_integralType),
                             (start + end) / 2.0, area);
            peakNum++;
        }

        if (!m_manualWarnings.isEmpty()) {
            QString text = QString("手动积分过程中发现 %1 处基线异常：\n\n").arg(m_manualWarnings.size());
            int showCount = qMin(10, m_manualWarnings.size());
            for (int i = 0; i < showCount; ++i) text += m_manualWarnings.at(i) + "\n";
            if (m_manualWarnings.size() > showCount)
                text += QString("\n...（共 %1 条，仅显示前 %2 条）").arg(m_manualWarnings.size()).arg(showCount);
            QMessageBox::warning(this, "积分警告", text);
        }

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

    m_statusLabel->setText("自动积分计算中...");
    m_statusLabel->setVisible(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    QElapsedTimer timer; timer.start();
    m_worker->setData(m_time, m_rawA, m_filtA, m_rawB, m_filtB, m_rawAB, m_filtAB);
    m_worker->setParameters(m_waveWidth, m_slopeThreshold, m_advance, m_delay,
                            m_integralType, m_visibility);
    m_worker->processAutoIntegration();
    qDebug() << "自动积分耗时:" << timer.elapsed() << "ms";
    QApplication::restoreOverrideCursor();
}

void DataProcessingDialog::onIntegrationFinished(const QList<QStringList> &results)
{
    m_resultTable->setRowCount(0);
    for (const QStringList &row : results) {
        int r = m_resultTable->rowCount();
        m_resultTable->insertRow(r);
        for (int c = 0; c < row.size() && c < 7; ++c) {
            m_resultTable->setItem(r, c, new QTableWidgetItem(row[c]));
        }
    }
    m_statusLabel->setText(QString("自动积分完成，共 %1 个峰。").arg(results.size()));
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::updateSettings(QMap<QString, bool> visibility,
                                          QString integralType,
                                          double waveWidth, double slopeThreshold,
                                          double advance, double delay, bool removeMean)
{
    m_visibility = visibility;
    m_integralType = integralType;
    m_waveWidth = waveWidth;
    m_slopeThreshold = slopeThreshold;
    m_advance = advance;
    m_delay = delay;

    bool meanChanged = (m_removeMean != removeMean);
    m_removeMean = removeMean;

    if (meanChanged) applyDataProcessing();
    else updatePlotVisibility();
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