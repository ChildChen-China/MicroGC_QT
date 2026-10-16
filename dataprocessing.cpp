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
#include <QFileInfo>
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
#include <QToolTip>
#include <QMouseEvent>
#include <QEvent>

#include "interactiveplot.h"
#include "qcustomplot.h"

//==========================================================
// 颜色分配
//==========================================================
QColor channelColor(int fileIdx, int channelIdx)
{
    // 文件基准色相：黄金角散布，跨文件差异大
    int baseHue = (fileIdx * 137) % 360;

    // 组内色相偏移：A组 +0°，B组 +40°，A-B组 +80°
    static const int groupOffset[3] = { 0, 40, 80 };

    int group = 0;
    bool filt = false;
    switch (channelIdx) {
    case CH_RAWA:   group = 0; filt = false; break;
    case CH_FILTA:  group = 0; filt = true;  break;
    case CH_RAWB:   group = 1; filt = false; break;
    case CH_FILTB:  group = 1; filt = true;  break;
    case CH_RAWAB:  group = 2; filt = false; break;
    case CH_FILTAB: group = 2; filt = true;  break;
    default: break;
    }

    int hue = (baseHue + groupOffset[group]) % 360;

    // 滤波前：深色（明度 90）；滤波后：浅色（明度 180）
    int lightness = filt ? 180 : 90;
    int sat       = filt ? 200 : 230;

    return QColor::fromHsl(hue, sat, lightness);
}

//==========================================================
// IntegralSettingsDialog
//==========================================================
IntegralSettingsDialog::IntegralSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("积分设置");
    setMinimumWidth(420);

    auto *mainLayout = new QVBoxLayout(this);

    // ============ 显示通道（动态） ============
    m_chkGroup = new QGroupBox("显示通道（打开文件后自动生成）", this);
    m_chkFormLayout = new QFormLayout(m_chkGroup);
    m_chkFormLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->addWidget(m_chkGroup);

    // ============ 积分参数 ============
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
    m_advance->setRange(-100.0, 100.0);
    m_advance->setDecimals(3);
    m_advance->setToolTip("波形提前(秒)：正数=起点向前延伸，负数=向后收缩");

    m_delay = new QDoubleSpinBox(paramGroup);
    m_delay->setRange(-100.0, 100.0);
    m_delay->setDecimals(3);
    m_delay->setToolTip("波形延后(秒)：正数=终点向后延伸，负数=向前收缩");

    m_chkRemoveMean = new QCheckBox("去均值(消除直流偏置)", paramGroup);

    m_normalizeMode = new QComboBox(paramGroup);
    m_normalizeMode->addItem("单曲线");
    m_normalizeMode->addItem("单文件");
    m_normalizeMode->addItem("多文件");
    m_normalizeMode->setToolTip(
        "单曲线：每条曲线内峰面积和=100%\n"
        "单文件：同一文件所有曲线峰面积和=100%\n"
        "多文件：所有文件所有曲线峰面积和=100%");

    paramForm->addRow("积分类型:", m_integralType);
    paramForm->addRow("波宽(s):", m_waveWidth);
    paramForm->addRow("斜率:", m_slopeThreshold);
    paramForm->addRow("波形提前:", m_advance);
    paramForm->addRow("波形延后:", m_delay);
    paramForm->addRow("归一化方式:", m_normalizeMode);
    paramForm->addRow("数据处理:", m_chkRemoveMean);
    mainLayout->addWidget(paramGroup);

    QPushButton *applyBtn = new QPushButton("应用", this);
    connect(applyBtn, &QPushButton::clicked, this, &IntegralSettingsDialog::applySettings);
    mainLayout->addWidget(applyBtn);

    // ============ 从 QSettings 加载 ============
    loadSettingsToWidgets();
}

void IntegralSettingsDialog::loadSettingsToWidgets()
{
    QSettings s("MyCompany", "MicroGC");

    QString typeStr = s.value("dp/integralType", "斜切").toString();
    int idx = m_integralType->findText(typeStr);
    m_integralType->setCurrentIndex(idx >= 0 ? idx : 0);

    m_waveWidth->setValue(s.value("dp/waveWidth", 0.6).toDouble());
    m_slopeThreshold->setValue(s.value("dp/slopeThreshold", 50.0).toDouble());
    m_advance->setValue(s.value("dp/advance", 0.0).toDouble());
    m_delay->setValue(s.value("dp/delay", 0.0).toDouble());
    m_chkRemoveMean->setChecked(s.value("dp/removeMean", false).toBool());

    QString norm = s.value("dp/normalizeMode", "单曲线").toString();
    int ni = m_normalizeMode->findText(norm);
    m_normalizeMode->setCurrentIndex(ni >= 0 ? ni : 0);
}

void IntegralSettingsDialog::saveSettingsFromWidgets()
{
    QSettings s("MyCompany", "MicroGC");
    s.setValue("dp/integralType",    m_integralType->currentText());
    s.setValue("dp/waveWidth",       m_waveWidth->value());
    s.setValue("dp/slopeThreshold",  m_slopeThreshold->value());
    s.setValue("dp/advance",         m_advance->value());
    s.setValue("dp/delay",           m_delay->value());
    s.setValue("dp/removeMean",      m_chkRemoveMean->isChecked());
    s.setValue("dp/normalizeMode",   m_normalizeMode->currentText());
    s.sync();
}

void IntegralSettingsDialog::setFiles(const QList<FileEntry> &files)
{
    m_fileNames.clear();
    m_fileVis.clear();

    for (const FileEntry &f : files) {
        m_fileNames.append(f.displayName);
        QList<bool> v;
        for (int i = 0; i < 6; ++i) v.append(f.visibility[i]);
        m_fileVis.append(v);
    }

    rebuildChannelChecks();
}

void IntegralSettingsDialog::rebuildChannelChecks()
{
    // 清空旧复选框
    while (m_chkFormLayout->rowCount() > 0) {
        m_chkFormLayout->removeRow(0);
    }
    m_chkList.clear();

    if (m_fileNames.isEmpty()) {
        auto *hint = new QLabel("（暂无文件）", m_chkGroup);
        hint->setStyleSheet("color: gray;");
        m_chkFormLayout->addRow(hint);
        return;
    }

    // 每个文件建一个带边框的小组
    for (int fi = 0; fi < m_fileNames.size(); ++fi) {
        auto *fileBox = new QGroupBox(m_fileNames[fi], m_chkGroup);
        auto *grid = new QGridLayout(fileBox);
        grid->setContentsMargins(6, 6, 6, 6);
        grid->setSpacing(4);

        QList<QCheckBox*> chkList;
        for (int ci = 0; ci < 6; ++ci) {
            auto *chk = new QCheckBox(channelName(ci), fileBox);
            bool v = (fi < m_fileVis.size() && ci < m_fileVis[fi].size())
                         ? m_fileVis[fi][ci]
                         : (ci % 2 == 1);   // 默认滤波后勾选
            chk->setChecked(v);
            grid->addWidget(chk, ci % 2, ci / 2);   // 每列上下两个
            chkList.append(chk);
        }
        m_chkList.append(chkList);
        m_chkFormLayout->addRow(fileBox);
    }
}

void IntegralSettingsDialog::applySettings()
{
    // 收集可见性
    QList<QList<bool>> vis;
    for (int fi = 0; fi < m_chkList.size(); ++fi) {
        QList<bool> v;
        for (QCheckBox *chk : m_chkList[fi]) v.append(chk->isChecked());
        vis.append(v);
    }

    saveSettingsFromWidgets();

    emit settingsChanged(m_integralType->currentText(),
                         m_waveWidth->value(),
                         m_slopeThreshold->value(),
                         m_advance->value(),
                         m_delay->value(),
                         m_chkRemoveMean->isChecked(),
                         m_normalizeMode->currentText(),
                         vis);
    hide();
}

//==========================================================
// IntegrationWorker
//==========================================================
IntegrationWorker::IntegrationWorker(QObject *parent) : QObject(parent) {}

void IntegrationWorker::setFiles(const QList<FileEntry> &files)
{
    m_files = files;
}

void IntegrationWorker::setParameters(double waveWidth, double slopeThreshold,
                                      double advance, double delay,
                                      const QString &integralType,
                                      const QString &normalizeMode)
{
    m_waveWidth = waveWidth;
    m_slopeThreshold = slopeThreshold;
    m_advance = advance;
    m_delay = delay;
    m_integralType = integralType;
    m_normalizeMode = normalizeMode;
}

void IntegrationWorker::processAutoIntegration()
{
    m_warnings.clear();
    QList<PeakItem> peaks;

    for (int fi = 0; fi < m_files.size(); ++fi) {
        const FileEntry &f = m_files[fi];
        for (int ci = 0; ci < 6; ++ci) {
            if (!f.visibility[ci]) continue;
            if (f.channels[ci].isEmpty()) continue;

            integrateChannel(f.time, f.channels[ci],
                             f.displayName, channelName(ci),
                             fi, ci, peaks);
        }
    }

    // ============ 归一化 ============
    if (m_normalizeMode == "单曲线") {
        // 按 (fileIdx, channelIdx) 分组
        QMap<QPair<int,int>, double> sumByCurve;
        for (const PeakItem &p : peaks) {
            sumByCurve[qMakePair(p.fileIdx, p.channelIdx)] += p.area;
        }
        QList<QStringList> results;
        for (const PeakItem &p : peaks) {
            double tot = sumByCurve.value(qMakePair(p.fileIdx, p.channelIdx), 0.0);
            double norm = (tot > 1e-12) ? (p.area / tot * 100.0) : 0.0;
            QStringList row = p.row;
            row << QString::number(norm, 'f', 2);
            results.append(row);
        }
        emit integrationFinished(results);
    }
    else if (m_normalizeMode == "单文件") {
        QMap<int, double> sumByFile;
        for (const PeakItem &p : peaks) sumByFile[p.fileIdx] += p.area;

        QList<QStringList> results;
        for (const PeakItem &p : peaks) {
            double tot = sumByFile.value(p.fileIdx, 0.0);
            double norm = (tot > 1e-12) ? (p.area / tot * 100.0) : 0.0;
            QStringList row = p.row;
            row << QString::number(norm, 'f', 2);
            results.append(row);
        }
        emit integrationFinished(results);
    }
    else {   // 多文件
        double total = 0.0;
        for (const PeakItem &p : peaks) total += p.area;

        QList<QStringList> results;
        for (const PeakItem &p : peaks) {
            double norm = (total > 1e-12) ? (p.area / total * 100.0) : 0.0;
            QStringList row = p.row;
            row << QString::number(norm, 'f', 2);
            results.append(row);
        }
        emit integrationFinished(results);
    }

    if (!m_warnings.isEmpty()) emit warningsGenerated(m_warnings);
}

void IntegrationWorker::integrateChannel(const QVector<double> &x, const QVector<double> &y,
                                         const QString &fileLabel, const QString &channelLabel,
                                         int fileIdx, int channelIdx,
                                         QList<PeakItem> &peaks)
{
    int n = x.size();
    if (n < 10) return;

    double dt = 0.0;
    if (n > 1) dt = (x[n - 1] - x[0]) / (n - 1);

    const double riseThreshold = m_slopeThreshold;
    const double fallThreshold = m_slopeThreshold;
    const double flatThreshold = m_slopeThreshold * 0.3;
    const int    confirmPoints = 3;
    const int    maxSearchBack    = 5000;
    const int    maxSearchForward = 5000;

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

        // 1. SS
        bool isSS = true;
        for (int k = 0; k < confirmPoints; ++k) {
            if (i + k >= n - 1) { isSS = false; break; }
            if (slopeAt(i + k) <= riseThreshold) { isSS = false; break; }
        }
        if (!isSS) { i++; continue; }
        int ssIdx = i;

        // 2. 峰顶
        double tempRange = localDataRange(qMax(0, ssIdx - 200), qMin(n - 1, ssIdx + 200));
        double tempDrop = qMax(0.5, tempRange * 0.05);

        int peakIdx = ssIdx;
        double localMaxY = y[ssIdx];
        for (int k = ssIdx + 1; k < n; ++k) {
            if (qIsNaN(y[k])) break;
            if (y[k] >= localMaxY) { localMaxY = y[k]; peakIdx = k; }
            else if (localMaxY - y[k] >= tempDrop) break;
        }

        double localRange = localDataRange(peakIdx - localWindowHalf,
                                           peakIdx + localWindowHalf);
        double minPeakHeight = qMax(0.5, localRange * 0.03);

        double peakHeight = localMaxY - y[ssIdx];
        if (peakHeight < minPeakHeight) {
            i = qMax(i + 1, peakIdx + 1);
            continue;
        }

        // 3. 起点
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

        // 4. ES
        int esIdx = -1;
        for (int k = peakIdx + 1; k < n - confirmPoints; ++k) {
            bool isES = true;
            for (int m = 0; m < confirmPoints; ++m) {
                if (slopeAt(k + m) >= -fallThreshold) { isES = false; break; }
            }
            if (isES) { esIdx = k; break; }
        }
        if (esIdx < 0) { i = qMax(i + 1, peakIdx + 1); continue; }

        // 5. 终点
        int endIdx = esIdx;
        bool foundE = false;
        int forwardLimit = qMin(n - 1 - esIdx, maxSearchForward);
        for (int k = esIdx + 1; k <= esIdx + forwardLimit; ++k) {
            if (qAbs(smoothedSlope(k)) < flatThreshold) {
                endIdx = k; foundE = true; break;
            }
        }
        if (!foundE) endIdx = qMin(n - 1, esIdx + forwardLimit);

        // 波宽过滤
        double rawPeakWidth = x[endIdx] - x[startIdx];
        if (rawPeakWidth < m_waveWidth) {
            i = qMax(i + 1, endIdx + 1);
            continue;
        }

        // advance / delay 作用
        double adjStartTime = x[startIdx] - m_advance;
        double adjEndTime   = x[endIdx]   + m_delay;

        int adjStartIdx = qBound(0, findIdxGE(adjStartTime), n - 1);
        int adjEndIdx   = qBound(0, findIdxLE(adjEndTime),   n - 1);
        if (adjStartIdx >= adjEndIdx) {
            i = qMax(i + 1, endIdx + 1);
            continue;
        }

        double area = calculateArea(x, y, adjStartIdx, adjEndIdx, m_integralType);

        PeakItem item;
        item.row << fileLabel                                          // 文件名
                 << QString("%1 峰%2").arg(channelLabel).arg(++peakCount)  // 组份
                 << QString::number(x[adjStartIdx], 'f', 3)            // 起始时间
                 << QString::number(x[adjEndIdx],   'f', 3)            // 结束时间
                 << "自动"                                             // 类型
                 << QString::number(x[peakIdx], 'f', 3)                // 保留时间
                 << QString::number(area, 'f', 3);                     // 峰面积
        item.fileIdx = fileIdx;
        item.channelIdx = channelIdx;
        item.area = area;
        peaks.append(item);

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
    if (type.contains("竖切")) {
        slope = 0.0;
        baselineY = qMin(y0, y1);
    } else {
        if (qAbs(x1 - x0) < 1e-12) slope = 0.0;
        else slope = (y1 - y0) / (x1 - x0);
        baselineY = y0;
    }

    // 面积 = |y - baseline| 逐段累加（不分正负半轴）
    double areaAbs = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i + 1] - x[i];
        if (qFuzzyIsNull(h)) continue;

        double baseline1, baseline2;
        if (type.contains("竖切")) {
            baseline1 = baselineY;
            baseline2 = baselineY;
        } else {
            baseline1 = baselineY + slope * (x[i]     - x0);
            baseline2 = baselineY + slope * (x[i + 1] - x0);
        }

        double val1 = y[i]     - baseline1;
        double val2 = y[i + 1] - baseline2;

        areaAbs += h * (qAbs(val1) + qAbs(val2)) / 2.0;
    }

    return areaAbs;
}

//==========================================================
// DataProcessingDialog
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
    resize(1300, 850);
    setAttribute(Qt::WA_DeleteOnClose);

    // QSettings 加载参数
    {
        QSettings s("MyCompany", "MicroGC");
        m_integralType   = s.value("dp/integralType",   "斜切").toString();
        m_waveWidth      = s.value("dp/waveWidth",      0.6).toDouble();
        m_slopeThreshold = s.value("dp/slopeThreshold", 50.0).toDouble();
        m_advance        = s.value("dp/advance",        0.0).toDouble();
        m_delay          = s.value("dp/delay",          0.0).toDouble();
        m_removeMean     = s.value("dp/removeMean",     false).toBool();
        m_normalizeMode  = s.value("dp/normalizeMode",  "单曲线").toString();
    }

    auto *mainLayout = new QVBoxLayout(this);

    // ============ 顶部按钮行 ============
    auto *btnLayout = new QHBoxLayout;
    QPushButton *openBtn = new QPushButton("打开文件", this);
    QPushButton *settingsBtn = new QPushButton("积分设置", this);
    QPushButton *autoIntegrateBtn = new QPushButton("自动积分", this);
    m_manualIntegralBtn = new QPushButton("手动积分", this);
    m_manualIntegralBtn->setCheckable(true);
    QPushButton *exportBtn = new QPushButton("导出", this);
    QPushButton *clearBtn = new QPushButton("清空图像", this);
    QPushButton *restoreBtn = new QPushButton("还原图像", this);

    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(settingsBtn);
    btnLayout->addWidget(autoIntegrateBtn);
    btnLayout->addWidget(m_manualIntegralBtn);
    btnLayout->addWidget(exportBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addWidget(restoreBtn);
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

    btnLayout->addWidget(zoomX);
    btnLayout->addWidget(zoomY);
    btnLayout->addWidget(zoomXY);
    btnLayout->addWidget(zoomRect);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    // ============ 状态标签 ============
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: blue; font-weight: bold;");
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);

    // ============ 绘图 ============
    m_plot = new InteractivePlot(this);
    m_plot->xAxis->setLabel("Time(s)");
    m_plot->yAxis->setLabel("Signal(uV)");
    setupPlots();
    m_plot->installEventFilter(this);     // 用于 hover 提示
    m_plot->setMouseTracking(true);
    mainLayout->addWidget(m_plot, 1);

    // 手动积分竖线
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

    // ============ 结果表格 ============
    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(8);
    m_resultTable->setHorizontalHeaderLabels({
        "文件名", "组份", "起始时间(s)", "结束时间(s)", "类型",
        "保留时间(s)", "峰面积(s*uV)", "归一化(%)"
    });
    {
        auto *h = m_resultTable->horizontalHeader();
        h->setStretchLastSection(false);
        h->setSectionResizeMode(0, QHeaderView::Stretch);            // 文件名
        h->setSectionResizeMode(1, QHeaderView::Stretch);            // 组份
        h->setSectionResizeMode(2, QHeaderView::ResizeToContents);   // 起始时间
        h->setSectionResizeMode(3, QHeaderView::ResizeToContents);   // 结束时间
        h->setSectionResizeMode(4, QHeaderView::ResizeToContents);   // 类型
        h->setSectionResizeMode(5, QHeaderView::ResizeToContents);   // 保留时间
        h->setSectionResizeMode(6, QHeaderView::Stretch);            // 峰面积
        h->setSectionResizeMode(7, QHeaderView::ResizeToContents);   // 归一化
    }
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setMinimumHeight(180);
    mainLayout->addWidget(m_resultTable, 0);

    // ============ 设置对话框 ============
    m_settingsDialog = new IntegralSettingsDialog(this);
    m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);

    // ============ 积分 worker ============
    m_worker = new IntegrationWorker(this);

    // ============ 信号连接 ============
    connect(openBtn, &QPushButton::clicked, this, &DataProcessingDialog::openFile);
    connect(settingsBtn, &QPushButton::clicked, this, &DataProcessingDialog::showIntegralSettings);
    connect(autoIntegrateBtn, &QPushButton::clicked, this, &DataProcessingDialog::autoIntegrate);
    connect(m_manualIntegralBtn, &QPushButton::toggled, this, &DataProcessingDialog::toggleManualIntegral);
    connect(exportBtn, &QPushButton::clicked, this, &DataProcessingDialog::exportData);
    connect(clearBtn, &QPushButton::clicked, this, &DataProcessingDialog::clearAll);
    connect(restoreBtn, &QPushButton::clicked, this, &DataProcessingDialog::resetView);
    connect(m_zoomGroup, qOverload<int>(&QButtonGroup::idClicked), this, &DataProcessingDialog::applyZoomMode);
    connect(m_plot, &InteractivePlot::rightClicked, this, &DataProcessingDialog::handleRightClick);
    connect(m_settingsDialog, &IntegralSettingsDialog::settingsChanged,
            this, &DataProcessingDialog::updateSettings);

    connect(m_worker, &IntegrationWorker::integrationFinished,
            this, &DataProcessingDialog::onIntegrationFinished);
    connect(m_worker, &IntegrationWorker::warningsGenerated, this,
            [this](const QList<QString> &warnings) {
                if (warnings.isEmpty()) return;
                QString text = QString("自动积分过程中发现 %1 处基线异常：\n\n").arg(warnings.size());
                int showCount = qMin(10, warnings.size());
                for (int i = 0; i < showCount; ++i) text += warnings.at(i) + "\n";
                if (warnings.size() > showCount)
                    text += QString("\n...（共 %1 条，仅显示前 %2 条）")
                                .arg(warnings.size()).arg(showCount);
                QMessageBox::warning(this, "积分警告", text);
            });
}

DataProcessingDialog::~DataProcessingDialog() {}

bool DataProcessingDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_plot && event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        handlePlotHover(me->pos());
    }
    return QDialog::eventFilter(watched, event);
}

void DataProcessingDialog::setupPlots()
{
    m_plot->clearGraphs();
    m_plot->setPlottingHints(QCP::phFastPolylines);
    m_plot->setNoAntialiasingOnDrag(true);

    // 隐藏图例（按需求：不再显示右上角的颜色说明）
    m_plot->legend->setVisible(false);

    m_plot->xAxis->setRange(0, 5);
    m_plot->yAxis->setRange(0, 5);
    m_plot->replot();
}

QVector<double> DataProcessingDialog::processedChannel(const FileEntry &f, int chIdx) const
{
    QVector<double> data = f.channels[chIdx];
    if (!m_removeMean) return data;

    double mean = 0.0;
    int cnt = 0;
    for (double v : data) {
        if (!qIsNaN(v)) { mean += v; cnt++; }
    }
    if (cnt > 0) mean /= cnt;
    for (double &v : data) v -= mean;
    return data;
}

void DataProcessingDialog::rebuildGraphs()
{
    m_plot->clearGraphs();

    for (int fi = 0; fi < m_files.size(); ++fi) {
        const FileEntry &f = m_files[fi];
        for (int ci = 0; ci < 6; ++ci) {
            QCPGraph *g = m_plot->addGraph();
            QColor color = channelColor(fi, ci);
            g->setPen(QPen(color, 2));
            g->setName(QString("%1 / %2").arg(f.displayName, channelName(ci)));

            QVector<double> data = processedChannel(f, ci);
            g->setData(f.time, data);
            g->setVisible(f.visibility[ci]);
        }
    }

    m_plot->replot();
}

void DataProcessingDialog::applyDataProcessing()
{
    if (m_files.isEmpty()) {
        m_plot->replot();
        return;
    }
    // 只更新数据，不重建图，保持缩放
    for (int fi = 0; fi < m_files.size(); ++fi) {
        const FileEntry &f = m_files[fi];
        for (int ci = 0; ci < 6; ++ci) {
            int gi = fi * 6 + ci;
            if (gi >= m_plot->graphCount()) break;
            QCPGraph *g = m_plot->graph(gi);
            QVector<double> data = processedChannel(f, ci);
            g->setData(f.time, data);
        }
    }
    m_plot->rescaleAxes();
    m_plot->replot();
}

void DataProcessingDialog::updatePlotVisibility()
{
    for (int gi = 0; gi < m_plot->graphCount(); ++gi) {
        int fi = gi / 6;
        int ci = gi % 6;
        if (fi >= m_files.size()) break;
        m_plot->graph(gi)->setVisible(m_files[fi].visibility[ci]);
    }
    m_plot->replot();
}

bool DataProcessingDialog::parseFile(const QString &path, FileEntry &out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    out.clear();
    out.filePath = path;
    out.displayName = QFileInfo(path).fileName();

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
        if (!okTime) continue;

        bool ok = false;
        double rawA  = parseValue(parts[1], ok); if (!ok) continue;
        double filtA = parseValue(parts[2], ok); if (!ok) continue;
        double rawB  = parseValue(parts[3], ok); if (!ok) continue;
        double filtB = parseValue(parts[4], ok); if (!ok) continue;
        double rawAB = parseValue(parts[5], ok); if (!ok) continue;
        double filtAB= parseValue(parts[6], ok); if (!ok) continue;

        out.time.append(timeSec);
        out.channels[CH_RAWA].append(rawA);
        out.channels[CH_FILTA].append(filtA);
        out.channels[CH_RAWB].append(rawB);
        out.channels[CH_FILTB].append(filtB);
        out.channels[CH_RAWAB].append(rawAB);
        out.channels[CH_FILTAB].append(filtAB);
        validLines++;
    }

    file.close();
    return validLines > 0;
}

void DataProcessingDialog::openFile()
{
    QStringList paths = QFileDialog::getOpenFileNames(
        this, "打开数据文件（可多选）", QString(),
        "文本文件 (*.txt);;所有文件 (*)");

    if (paths.isEmpty()) return;

    int addedCount = 0;
    int skippedCount = 0;

    for (const QString &path : paths) {
        // 查重
        bool dup = false;
        for (const FileEntry &f : m_files) {
            if (f.filePath == path) { dup = true; break; }
        }
        if (dup) { skippedCount++; continue; }

        FileEntry f;
        if (parseFile(path, f)) {
            m_files.append(f);
            addedCount++;
        }
    }

    if (addedCount == 0) {
        m_statusLabel->setText(QString("没有新文件被加载（跳过重复 %1 个）").arg(skippedCount));
        m_statusLabel->setVisible(true);
        return;
    }

    rebuildGraphs();
    m_settingsDialog->setFiles(m_files);

    m_statusLabel->setText(QString("已加载 %1 个文件，共 %2 个文件")
                               .arg(addedCount).arg(m_files.size()));
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::clearAll()
{
    m_files.clear();
    m_plot->clearGraphs();
    m_resultTable->setRowCount(0);
    m_plot->replot();

    m_settingsDialog->setFiles(m_files);

    m_statusLabel->setText("已清空全部文件和图像");
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::showIntegralSettings()
{
    m_settingsDialog->setFiles(m_files);
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
        return;
    }

    double endPoint = scenePos.x();
    m_endLine->point1->setCoords(endPoint, m_plot->yAxis->range().lower);
    m_endLine->point2->setCoords(endPoint, m_plot->yAxis->range().upper);
    m_endLine->setVisible(true);
    m_plot->replot();

    double start = qMin(m_startPoint, endPoint);
    double end   = qMax(m_startPoint, endPoint);

    // 用于归一化统计
    struct ManualPeak {
        QString label;
        QString curveLabel;
        double start, end, area;
        int fileIdx, channelIdx;
    };
    QList<ManualPeak> all;

    for (int fi = 0; fi < m_files.size(); ++fi) {
        const FileEntry &f = m_files[fi];
        for (int ci = 0; ci < 6; ++ci) {
            if (!f.visibility[ci]) continue;
            if (f.channels[ci].isEmpty()) continue;

            int startIdx = -1, endIdx = -1;
            for (int k = 0; k < f.time.size(); ++k) {
                if (f.time[k] >= start && startIdx < 0) startIdx = k;
                if (f.time[k] <= end) endIdx = k;
            }
            if (startIdx < 0 || endIdx < 0 || startIdx >= endIdx) continue;

            QVector<double> data = processedChannel(f, ci);
            double area = calculateArea(f.time, data, startIdx, endIdx, m_integralType);
            if (qAbs(area) < 1e-9) continue;

            ManualPeak mp;
            mp.label       = f.displayName;             // 文件名
            mp.curveLabel  = channelName(ci);           // 通道名
            mp.start = start;
            mp.end = end;
            mp.area = area;
            mp.fileIdx = fi;
            mp.channelIdx = ci;
            all.append(mp);
        }
    }

    if (all.isEmpty()) {
        m_statusLabel->setText("该范围内没有可见曲线");
        m_selectingStart = true;
        return;
    }

    // 按归一化模式计算比例
    QList<double> normList;
    normList.resize(all.size());

    if (m_normalizeMode == "单曲线") {
        QMap<QPair<int,int>, double> sumCurve;
        for (const auto &mp : all)
            sumCurve[qMakePair(mp.fileIdx, mp.channelIdx)] += mp.area;
        for (int i = 0; i < all.size(); ++i) {
            double tot = sumCurve.value(qMakePair(all[i].fileIdx, all[i].channelIdx), 0.0);
            normList[i] = (tot > 1e-12) ? (all[i].area / tot * 100.0) : 0.0;
        }
    } else if (m_normalizeMode == "单文件") {
        QMap<int, double> sumFile;
        for (const auto &mp : all) sumFile[mp.fileIdx] += mp.area;
        for (int i = 0; i < all.size(); ++i) {
            double tot = sumFile.value(all[i].fileIdx, 0.0);
            normList[i] = (tot > 1e-12) ? (all[i].area / tot * 100.0) : 0.0;
        }
    } else {
        double total = 0.0;
        for (const auto &mp : all) total += mp.area;
        for (int i = 0; i < all.size(); ++i) {
            normList[i] = (total > 1e-12) ? (all[i].area / total * 100.0) : 0.0;
        }
    }

    m_manualWarnings.clear();
    for (int i = 0; i < all.size(); ++i) {
        const auto &mp = all[i];
        addResultToTable(mp.label,                                     // 文件名
                         QString("%1 峰1").arg(mp.curveLabel),         // 组份
                         mp.start, mp.end,
                         "手动",                                        // 类型
                         (mp.start + mp.end) / 2.0,
                         mp.area,
                         normList[i]);
    }

    m_selectingStart = true;
    m_statusLabel->setText("积分完成，可继续右键选择新峰起点，或再次点击手动积分退出");
}

void DataProcessingDialog::autoIntegrate()
{
    if (m_files.isEmpty()) {
        m_statusLabel->setText("请先打开数据文件");
        m_statusLabel->setVisible(true);
        return;
    }

    // 检查是否有可见曲线
    bool anyVisible = false;
    for (const FileEntry &f : m_files) {
        for (int i = 0; i < 6; ++i) if (f.visibility[i]) { anyVisible = true; break; }
        if (anyVisible) break;
    }
    if (!anyVisible) {
        m_statusLabel->setText("没有任何可见曲线（请在积分设置中勾选）");
        m_statusLabel->setVisible(true);
        return;
    }

    m_statusLabel->setText("自动积分计算中...");
    m_statusLabel->setVisible(true);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    QElapsedTimer timer; timer.start();

    // 构造去均值后的临时文件副本传给 worker
    QList<FileEntry> workFiles = m_files;
    if (m_removeMean) {
        for (FileEntry &f : workFiles) {
            for (int ci = 0; ci < 6; ++ci) {
                double mean = 0.0;
                int cnt = 0;
                for (double v : f.channels[ci]) {
                    if (!qIsNaN(v)) { mean += v; cnt++; }
                }
                if (cnt > 0) mean /= cnt;
                for (double &v : f.channels[ci]) v -= mean;
            }
        }
    }

    m_worker->setFiles(workFiles);
    m_worker->setParameters(m_waveWidth, m_slopeThreshold,
                            m_advance, m_delay,
                            m_integralType, m_normalizeMode);
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
        for (int c = 0; c < row.size() && c < 8; ++c) {
            m_resultTable->setItem(r, c, new QTableWidgetItem(row[c]));
        }
    }
    m_statusLabel->setText(QString("自动积分完成，共 %1 个峰。").arg(results.size()));
    m_statusLabel->setVisible(true);
}

void DataProcessingDialog::updateSettings(QString integralType,
                                          double waveWidth, double slopeThreshold,
                                          double advance, double delay,
                                          bool removeMean,
                                          QString normalizeMode,
                                          QList<QList<bool>> visibility)
{
    m_integralType = integralType;
    m_waveWidth = waveWidth;
    m_slopeThreshold = slopeThreshold;
    m_advance = advance;
    m_delay = delay;
    m_normalizeMode = normalizeMode;

    bool meanChanged = (m_removeMean != removeMean);
    m_removeMean = removeMean;

    // 更新每个文件的可见性
    for (int fi = 0; fi < m_files.size() && fi < visibility.size(); ++fi) {
        for (int ci = 0; ci < 6 && ci < visibility[fi].size(); ++ci) {
            m_files[fi].visibility[ci] = visibility[fi][ci];
        }
    }

    if (meanChanged) applyDataProcessing();
    else updatePlotVisibility();
}

void DataProcessingDialog::handlePlotHover(const QPoint &pos)
{
    if (m_files.isEmpty() || m_plot->graphCount() == 0) {
        QToolTip::hideText();
        return;
    }

    QCPGraph *best = nullptr;
    double bestDist = 12.0;   // 像素容差

    // 将鼠标左右各 20 像素转为数据坐标，作为扫描窗口
    double xLeft  = m_plot->xAxis->pixelToCoord(pos.x() - 20);
    double xRight = m_plot->xAxis->pixelToCoord(pos.x() + 20);

    for (int gi = 0; gi < m_plot->graphCount(); ++gi) {
        QCPGraph *g = m_plot->graph(gi);
        if (!g || !g->visible()) continue;

        QSharedPointer<QCPGraphDataContainer> data = g->data();
        if (data->isEmpty()) continue;

        auto itBegin = data->findBegin(xLeft,  true);
        auto itEnd   = data->findEnd(xRight, true);

        for (auto it = itBegin; it != itEnd; ++it) {
            double px = m_plot->xAxis->coordToPixel(it->key);
            double py = m_plot->yAxis->coordToPixel(it->value);
            double dx = px - pos.x();
            double dy = py - pos.y();
            double d  = qSqrt(dx * dx + dy * dy);
            if (d < bestDist) {
                bestDist = d;
                best = g;
            }
        }
    }

    if (best) {
        QToolTip::showText(m_plot->mapToGlobal(pos), best->name(), m_plot);
    } else {
        QToolTip::hideText();
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
    if (type.contains("竖切")) {
        slope = 0.0;
        baselineY = qMin(y0, y1);
    } else {
        if (qAbs(x1 - x0) < 1e-12) slope = 0.0;
        else slope = (y1 - y0) / (x1 - x0);
        baselineY = y0;
    }

    double areaAbs = 0.0;
    for (int i = startIdx; i < endIdx; ++i) {
        double h = x[i + 1] - x[i];
        if (qFuzzyIsNull(h)) continue;

        double baseline1, baseline2;
        if (type.contains("竖切")) {
            baseline1 = baselineY;
            baseline2 = baselineY;
        } else {
            baseline1 = baselineY + slope * (x[i]     - x0);
            baseline2 = baselineY + slope * (x[i + 1] - x0);
        }

        double val1 = y[i]     - baseline1;
        double val2 = y[i + 1] - baseline2;

        areaAbs += h * (qAbs(val1) + qAbs(val2)) / 2.0;
    }

    return areaAbs;
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
                                            double retention, double area, double normalized)
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
    m_resultTable->setItem(row, 7, new QTableWidgetItem(QString::number(normalized, 'f', 2)));
}