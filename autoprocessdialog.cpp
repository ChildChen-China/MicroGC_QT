#include "autoprocessdialog.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <algorithm>
#include <functional>

//==========================================================
// TimelineWidget
//==========================================================
TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TimelineWidget::setGroups(const QList<AutoEventGroup> &groups)
{
    m_groups = groups;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    p.setPen(QPen(QColor(200, 200, 200), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    if (m_groups.isEmpty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, "（暂无事件，请添加）");
        return;
    }

    // 计算最大时间
    double maxTime = 1.0;
    for (const auto &g : m_groups) {
        maxTime = qMax(maxTime, g.onTimeMin);
        maxTime = qMax(maxTime, g.offTimeMin);
    }
    // 向上取整到 0.5 的倍数
    maxTime = qCeil(maxTime * 2.0) / 2.0;

    const int leftMargin = 70;
    const int rightMargin = 20;
    const int topMargin = 15;
    const int bottomMargin = 40;

    int w = width() - leftMargin - rightMargin;
    int h = height() - topMargin - bottomMargin;
    if (w <= 0 || h <= 0) return;

    auto timeToX = [&](double t) -> int {
        return leftMargin + static_cast<int>(t / maxTime * w);
    };

    int laneCollect  = topMargin + h / 6;
    int laneValve    = topMargin + h / 2;
    int lanePolarity = topMargin + h * 5 / 6;

    // 时间轴
    int axisY = topMargin + h;
    p.setPen(QPen(Qt::black, 1.5));
    p.drawLine(leftMargin, axisY, leftMargin + w, axisY);

    // 刻度
    int numTicks = 5;
    for (int i = 0; i <= numTicks; ++i) {
        double t = maxTime * i / numTicks;
        int x = timeToX(t);
        p.setPen(QPen(Qt::gray, 1));
        p.drawLine(x, axisY, x, axisY + 5);
        p.setPen(Qt::black);
        p.drawText(x - 25, axisY + 8, 50, 16,
                   Qt::AlignCenter, QString::number(t, 'f', 2));
    }

    // 泳道标签
    p.setPen(Qt::black);
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(5, laneCollect - 8, 60, 16, Qt::AlignRight | Qt::AlignVCenter, "采集");
    p.drawText(5, laneValve - 8, 60, 16, Qt::AlignRight | Qt::AlignVCenter, "六通阀");
    p.drawText(5, lanePolarity - 8, 60, 16, Qt::AlignRight | Qt::AlignVCenter, "极性");

    // 事件节点
    for (const auto &g : m_groups) {
        int lane = 0;
        QColor onColor, offColor;

        switch (g.type) {
        case AutoEventType::Collect:
            lane = laneCollect;
            onColor = QColor(0, 150, 0);
            offColor = QColor(200, 0, 0);
            break;
        case AutoEventType::Valve:
            lane = laneValve;
            onColor = QColor(0, 0, 220);
            offColor = QColor(150, 0, 200);
            break;
        case AutoEventType::Polarity:
            lane = lanePolarity;
            onColor = QColor(210, 150, 0);
            offColor = QColor(0, 160, 160);
            break;
        }

        int x1 = timeToX(g.onTimeMin);
        int x2 = timeToX(g.offTimeMin);

        // 连接虚线
        p.setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
        p.drawLine(x1, lane, x2, lane);

        // 开节点
        p.setPen(QPen(onColor.darker(130), 1));
        p.setBrush(onColor);
        p.drawEllipse(QPoint(x1, lane), 6, 6);

        // 关节点
        p.setPen(QPen(offColor.darker(130), 1));
        p.setBrush(offColor);
        p.drawEllipse(QPoint(x2, lane), 6, 6);
    }
}

//==========================================================
// AutoProcessDialog
//==========================================================
AutoProcessDialog::AutoProcessDialog(AutoProcessManager *mgr, QWidget *parent)
    : QDialog(parent)
    , m_mgr(mgr)
{
    setWindowTitle("自动流程设置");
    resize(900, 640);

    auto *mainLayout = new QVBoxLayout(this);

    // ========== 顶部按钮 ==========
    auto *topBar = new QHBoxLayout;
    QPushButton *btnAddCollect  = new QPushButton("+ 数据采集", this);
    QPushButton *btnAddValve    = new QPushButton("+ 六通阀", this);
    QPushButton *btnAddPolarity = new QPushButton("+ 极性反转", this);
    QPushButton *btnRemove      = new QPushButton("- 删除选中", this);
    topBar->addWidget(btnAddCollect);
    topBar->addWidget(btnAddValve);
    topBar->addWidget(btnAddPolarity);
    topBar->addWidget(btnRemove);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    connect(btnAddCollect,  &QPushButton::clicked, this, &AutoProcessDialog::onAddCollect);
    connect(btnAddValve,    &QPushButton::clicked, this, &AutoProcessDialog::onAddValve);
    connect(btnAddPolarity, &QPushButton::clicked, this, &AutoProcessDialog::onAddPolarity);
    connect(btnRemove,      &QPushButton::clicked, this, &AutoProcessDialog::onRemoveSelected);

    // ========== 事件表格 ==========
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"类型", "开时间(min)", "关时间(min)", "操作"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(38);
    m_table->setMinimumHeight(200);
    mainLayout->addWidget(m_table, 1);

    // ========== 时间轴 ==========
    auto *timelineLabel = new QLabel("时间轴总览（左键拖动可查看，颜色区分开/关节点）：", this);
    mainLayout->addWidget(timelineLabel);
    m_timeline = new TimelineWidget(this);
    mainLayout->addWidget(m_timeline, 0);

    // ========== 文件保存/加载 ==========
    auto *fileBar = new QHBoxLayout;
    QPushButton *btnSave = new QPushButton("保存为文件...", this);
    QPushButton *btnLoad = new QPushButton("从文件加载...", this);
    fileBar->addWidget(btnSave);
    fileBar->addWidget(btnLoad);
    fileBar->addStretch();
    mainLayout->addLayout(fileBar);

    connect(btnSave, &QPushButton::clicked, this, &AutoProcessDialog::onSaveToFile);
    connect(btnLoad, &QPushButton::clicked, this, &AutoProcessDialog::onLoadFromFile);

    // ========== 底部按钮 ==========
    auto *bottomBar = new QHBoxLayout;
    QPushButton *btnOk     = new QPushButton("确定", this);
    QPushButton *btnCancel = new QPushButton("取消", this);
    bottomBar->addStretch();
    bottomBar->addWidget(btnOk);
    bottomBar->addWidget(btnCancel);
    mainLayout->addLayout(bottomBar);

    connect(btnOk,     &QPushButton::clicked, this, &AutoProcessDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    refreshTable();
}

//==========================================================
// 刷新表格
//==========================================================
void AutoProcessDialog::refreshTable()
{
    m_refreshing = true;

    auto groups = m_mgr->groups();
    m_table->setRowCount(groups.size());

    for (int i = 0; i < groups.size(); ++i) {
        const auto &g = groups[i];

        // ---- 类型标签 ----
        QString typeStr;
        QColor color;
        switch (g.type) {
        case AutoEventType::Collect:
            typeStr = "数据采集"; color = QColor(0, 130, 0); break;
        case AutoEventType::Valve:
            typeStr = "六通阀";   color = QColor(0, 0, 210); break;
        case AutoEventType::Polarity:
            typeStr = "极性反转"; color = QColor(210, 140, 0); break;
        }

        auto *typeLabel = new QLabel(typeStr, m_table);
        QFont f = typeLabel->font();
        f.setBold(true);
        typeLabel->setFont(f);
        QPalette pal = typeLabel->palette();
        pal.setColor(QPalette::WindowText, color);
        typeLabel->setPalette(pal);
        typeLabel->setAlignment(Qt::AlignCenter);
        m_table->setCellWidget(i, 0, typeLabel);

        // ---- 开时间 ----
        auto *onSpin = new QDoubleSpinBox(m_table);
        onSpin->setRange(0.0, 999.0);
        onSpin->setDecimals(3);
        onSpin->setSingleStep(0.01);
        onSpin->setValue(g.onTimeMin);
        onSpin->setSuffix(" min");
        m_table->setCellWidget(i, 1, onSpin);

        // ---- 关时间 ----
        auto *offSpin = new QDoubleSpinBox(m_table);
        offSpin->setRange(0.0, 999.0);
        offSpin->setDecimals(3);
        offSpin->setSingleStep(0.01);
        offSpin->setValue(g.offTimeMin);
        offSpin->setSuffix(" min");
        m_table->setCellWidget(i, 2, offSpin);

        // ---- 时间改变回调 ----
        connect(onSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this, i](double v) {
                    if (m_refreshing) return;
                    auto list = m_mgr->groups();
                    if (i >= 0 && i < list.size()) {
                        list[i].onTimeMin = v;
                        m_mgr->setGroups(list);
                        m_timeline->setGroups(list);
                    }
                });
        connect(offSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this, i](double v) {
                    if (m_refreshing) return;
                    auto list = m_mgr->groups();
                    if (i >= 0 && i < list.size()) {
                        list[i].offTimeMin = v;
                        m_mgr->setGroups(list);
                        m_timeline->setGroups(list);
                    }
                });

        // ---- 删除按钮 ----
        auto *delBtn = new QPushButton("删除", m_table);
        m_table->setCellWidget(i, 3, delBtn);
        connect(delBtn, &QPushButton::clicked, this, [this, i]() {
            auto list = m_mgr->groups();
            if (i >= 0 && i < list.size()) {
                if (list.size() <= 1) {
                    QMessageBox::information(this, "提示", "至少需要保留一组事件");
                    return;
                }
                list.removeAt(i);
                m_mgr->setGroups(list);
                refreshTable();
            }
        });
    }

    m_timeline->setGroups(groups);

    m_refreshing = false;
}

//==========================================================
// 添加事件
//==========================================================
void AutoProcessDialog::onAddCollect()
{
    auto list = m_mgr->groups();

    // 找最大时间，新组的开时间放在最大时间之后
    double maxT = 0.0;
    for (const auto &g : list) {
        maxT = qMax(maxT, g.offTimeMin);
    }

    AutoEventGroup g;
    g.type = AutoEventType::Collect;
    g.onTimeMin = list.isEmpty() ? 0.01 : (maxT + 0.1);
    g.offTimeMin = g.onTimeMin + 1.0;
    list.append(g);
    m_mgr->setGroups(list);
    refreshTable();
}

void AutoProcessDialog::onAddValve()
{
    auto list = m_mgr->groups();
    double maxT = 0.0;
    for (const auto &g : list) maxT = qMax(maxT, g.offTimeMin);

    AutoEventGroup g;
    g.type = AutoEventType::Valve;
    g.onTimeMin = maxT + 0.1;
    g.offTimeMin = g.onTimeMin + 1.0;
    list.append(g);
    m_mgr->setGroups(list);
    refreshTable();
}

void AutoProcessDialog::onAddPolarity()
{
    auto list = m_mgr->groups();
    double maxT = 0.0;
    for (const auto &g : list) maxT = qMax(maxT, g.offTimeMin);

    AutoEventGroup g;
    g.type = AutoEventType::Polarity;
    g.onTimeMin = maxT + 0.1;
    g.offTimeMin = g.onTimeMin + 1.0;
    list.append(g);
    m_mgr->setGroups(list);
    refreshTable();
}

void AutoProcessDialog::onRemoveSelected()
{
    auto selected = m_table->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选中要删除的行");
        return;
    }

    auto list = m_mgr->groups();
    if (list.size() - selected.size() < 1) {
        QMessageBox::information(this, "提示", "至少需要保留一组事件");
        return;
    }

    QList<int> rows;
    for (const auto &idx : selected) rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int r : rows) {
        if (r >= 0 && r < list.size()) list.removeAt(r);
    }

    m_mgr->setGroups(list);
    refreshTable();
}

//==========================================================
// 文件保存/加载
//==========================================================
void AutoProcessDialog::onSaveToFile()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "保存自动流程", QString(), "JSON 文件 (*.json)");
    if (fileName.isEmpty()) return;

    if (m_mgr->exportToFile(fileName))
        QMessageBox::information(this, "保存成功", "已保存到:\n" + fileName);
    else
        QMessageBox::warning(this, "保存失败", "无法写入文件");
}

void AutoProcessDialog::onLoadFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, "加载自动流程", QString(), "JSON 文件 (*.json)");
    if (fileName.isEmpty()) return;

    if (m_mgr->importFromFile(fileName)) {
        refreshTable();
        QMessageBox::information(this, "加载成功", "已加载:\n" + fileName);
    } else {
        QMessageBox::warning(this, "加载失败", "文件格式不正确或读取失败");
    }
}

//==========================================================
// 校验
//==========================================================
bool AutoProcessDialog::validate() const
{
    auto groups = m_mgr->groups();

    if (groups.isEmpty()) {
        QMessageBox::warning(const_cast<AutoProcessDialog*>(this),
                             "校验失败", "至少需要一组事件");
        return false;
    }

    bool hasCollect = false;
    for (const auto &g : groups) {
        if (g.type == AutoEventType::Collect) {
            hasCollect = true;
            break;
        }
    }
    if (!hasCollect) {
        QMessageBox::warning(const_cast<AutoProcessDialog*>(this),
                             "校验失败", "必须包含至少一组数据采集事件");
        return false;
    }

    for (int i = 0; i < groups.size(); ++i) {
        const auto &g = groups[i];
        if (g.onTimeMin < 0 || g.offTimeMin < 0) {
            QMessageBox::warning(const_cast<AutoProcessDialog*>(this),
                                 "校验失败",
                                 QString("第 %1 组时间不能为负").arg(i + 1));
            return false;
        }
        if (g.onTimeMin >= g.offTimeMin) {
            QMessageBox::warning(const_cast<AutoProcessDialog*>(this),
                                 "校验失败",
                                 QString("第 %1 组的开时间必须小于关时间").arg(i + 1));
            return false;
        }
    }

    return true;
}

void AutoProcessDialog::onAccept()
{
    if (!validate()) return;
    m_mgr->saveToSettings();
    accept();
}