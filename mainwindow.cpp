#include "mainwindow.h"
#include "logwidget.h"
#include "communication.h"
#include "controltab.h"
#include "monitortab.h"
#include "othertab.h"
#include "dataprocessing.h"
#include "autoprocessdialog.h"

#include <QToolBar>
#include <QAction>
#include <QToolButton>
#include <QMenu>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStyle>
#include <QSettings>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QFileDialog>
#include <QDateTime>
#include <QTimer>
#include <QPainter>
#include <QPixmap>
#include <QColor>
#include <QPen>
#include <QDoubleSpinBox>

#include <QSet>
#include <QMouseEvent>

//==========================================================
// 自定义 QMenu：指定 action 点击后菜单不关闭
//==========================================================
class StayOpenMenu : public QMenu
{
public:
    explicit StayOpenMenu(QWidget *parent = nullptr) : QMenu(parent) {}

    void setStayOpenActions(const QList<QAction*> &actions) {
        for (QAction *a : actions) m_stayOpen.insert(a);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        QAction *action = activeAction();
        if (action && m_stayOpen.contains(action)) {
            action->trigger();
            return;   // 不调用基类，菜单不关闭
        }
        QMenu::mouseReleaseEvent(e);
    }

private:
    QSet<QAction*> m_stayOpen;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabs(nullptr)
    , m_log(nullptr)
    , m_comm(nullptr)
    , m_monitorTab(nullptr)
    , m_controlTab(nullptr)
    , m_otherTab(nullptr)
    , m_autoProcess(nullptr)
    , m_startStopAction(nullptr)
    , m_tcdPowerAction(nullptr)
    , m_tcdCheckTimer(nullptr)
    , m_tcdPowered(false)
{
    setWindowTitle("MicroGC");
    resize(1300, 950);
    setMinimumSize(1200, 720);

    m_comm = new Communication(this);
    m_log = new LogWidget(this);

    // ========== TCD 电源状态图标 ==========
    {
        const int iconSize = 24;

        QPixmap offPixmap(iconSize, iconSize);
        offPixmap.fill(Qt::transparent);
        QPainter painterOff(&offPixmap);
        painterOff.setRenderHint(QPainter::Antialiasing);
        painterOff.setBrush(QColor(100, 100, 100));
        painterOff.setPen(QPen(Qt::black, 2));
        painterOff.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
        m_iconTcdOff = QIcon(offPixmap);

        QPixmap onPixmap(iconSize, iconSize);
        onPixmap.fill(Qt::transparent);
        QPainter painterOn(&onPixmap);
        painterOn.setRenderHint(QPainter::Antialiasing);
        painterOn.setBrush(QColor(0, 230, 64));
        painterOn.setPen(QPen(Qt::white, 2));
        painterOn.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
        painterOn.setBrush(Qt::white);
        painterOn.setPen(Qt::NoPen);
        painterOn.drawEllipse(8, 6, 5, 5);
        m_iconTcdOn = QIcon(onPixmap);
    }

    // ========== TC4 加热开关图标 ==========
    {
        const int iconSize = 24;

        QPixmap offPixmap(iconSize, iconSize);
        offPixmap.fill(Qt::transparent);
        QPainter painterOff(&offPixmap);
        painterOff.setRenderHint(QPainter::Antialiasing);
        painterOff.setBrush(QColor(120, 120, 120));
        painterOff.setPen(QPen(QColor(70, 70, 180), 2));
        painterOff.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
        m_iconOvenOff = QIcon(offPixmap);

        QPixmap onPixmap(iconSize, iconSize);
        onPixmap.fill(Qt::transparent);
        QPainter painterOn(&onPixmap);
        painterOn.setRenderHint(QPainter::Antialiasing);
        painterOn.setBrush(QColor(230, 126, 34));
        painterOn.setPen(QPen(Qt::white, 2));
        painterOn.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
        painterOn.setBrush(Qt::white);
        painterOn.setPen(Qt::NoPen);
        painterOn.drawEllipse(8, 6, 5, 5);
        m_iconOvenOn = QIcon(onPixmap);
    }

    // ========== TCD 电源状态检查定时器 ==========
    m_tcdCheckTimer = new QTimer(this);
    m_tcdCheckTimer->setInterval(1000);
    m_tcdCheckTimer->setTimerType(Qt::CoarseTimer);
    connect(m_tcdCheckTimer, &QTimer::timeout, this, &MainWindow::checkTcdPowerStatus);

    // ========== 自动流程 ==========
    m_autoProcess = new AutoProcessManager(m_comm, this);
    connect(m_autoProcess, &AutoProcessManager::logMessage, this,
            [this](const QString &type, const QString &event) {
                m_log->appendLog(type, event);
                m_log->appendFileOnly(type, event);
            });
    connect(m_autoProcess, &AutoProcessManager::stateChanged, this,
            [this](const QString &state) {
                QString msg = "状态: " + state;
                m_log->appendLog("自动流程", msg);
                m_log->appendFileOnly("自动流程", msg);
                if (m_startStopAction) {
                    if (state == "空闲") {
                        m_startStopAction->setText("开始测量");
                        m_startStopAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
                    } else {
                        m_startStopAction->setText("停止");
                        m_startStopAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
                    }
                }
                // 通知 ControlTab 自动流程的运行状态
                if (m_controlTab) {
                    m_controlTab->setAutoProcessRunning(state != "空闲");
                }
            });


    // ========== 通信信号 ==========
    connect(m_comm, &Communication::logPacket, this,
            [this](const QString &direction, const QString &dataHex) {
                QString msg = QString("[%1] %2").arg(direction, dataHex);
                m_log->appendLog("通信", msg);
                m_log->appendFileOnly("通信", msg);
            });
    connect(m_comm, &Communication::statusMessage, this, [this](const QString &msg) {
        m_log->appendLog("通信", msg);
        m_log->appendFileOnly("通信", msg);
    });
    connect(m_comm, &Communication::connected, this, &MainWindow::onCommunicationConnected);
    connect(m_comm, &Communication::disconnected, this, &MainWindow::onCommunicationDisconnected);

    // 0x000A 故障位变化
    connect(m_comm, &Communication::slowDataUpdated, this, [this]() {
        quint16 diag = m_comm->diagRegister();
        if (diag != m_lastDiag) {
            m_lastDiag = diag;
            onFaultBitsChanged(diag);
        }
    });

    // 自动流程设备状态（六通阀/电磁阀图形更新）
    connect(m_autoProcess, &AutoProcessManager::deviceStateChanged,
            this, [this](const QString &device, bool state) {
                if (device == "六通阀" || device == "电磁阀") {
                    if (m_controlTab) m_controlTab->updateDeviceState(device, state);
                }
            });

    createActions();
    createTabs();

    // ==========================================================
    // 启动时立即应用 QSettings 中保存的"全局参数"
    // 保证每次打开软件，采集点数 / 均点数就与上次关闭一致
    // ==========================================================
    {
        QSettings s("MyCompany", "MicroGC");

        int collectPoints = s.value("global/collectPoints", 1000).toInt();
        int averagePoints = s.value("global/averagePoints", 5).toInt();

        if (m_monitorTab) {
            m_monitorTab->setCollectPoints(collectPoints);
            m_monitorTab->setAveragePoints(averagePoints);
        }
    }

    m_tcdCheckTimer->start();
    if (m_autoProcess && m_monitorTab) {
        m_autoProcess->setTcdTimeProvider([this]() -> double {
            return m_monitorTab ? m_monitorTab->currentTcdSeconds() : 0.0;
        });
    }

    // ========== 自动流程 → MonitorTab ==========
    connect(m_autoProcess, &AutoProcessManager::startDataSaveTriggered,
            m_monitorTab, &MonitorTab::startAutoDataSave);
    connect(m_autoProcess, &AutoProcessManager::stopDataSaveTriggered,
            m_monitorTab, &MonitorTab::stopDataSave);
    connect(m_autoProcess, &AutoProcessManager::polarityOnTriggered,
            m_monitorTab, &MonitorTab::startPolarityReversal);
    connect(m_autoProcess, &AutoProcessManager::polarityOffTriggered,
            m_monitorTab, &MonitorTab::stopPolarityReversal);


    // ========== 中央布局 ==========
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_tabs, 1);
    m_log->setMaximumHeight(220);
    layout->addWidget(m_log, 0);
    setCentralWidget(central);
}


void MainWindow::createActions()
{
    QToolBar *toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setIconSize(QSize(24, 24));

    // ========== 连接按钮 ==========
    QAction *connectAct = toolbar->addAction(style()->standardIcon(QStyle::SP_DriveNetIcon), "连接");
    m_connectAction = connectAct;
    m_iconConnectDefault = connectAct->icon();

    // ========== 设置下拉菜单 ==========
    QToolButton *settingsBtn = new QToolButton(this);
    settingsBtn->setText("设置");
    settingsBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
    settingsBtn->setPopupMode(QToolButton::InstantPopup);
    settingsBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    StayOpenMenu *settingsMenu = new StayOpenMenu(settingsBtn);
    QAction *globalSettingsAct = settingsMenu->addAction("全局参数设置");
    settingsMenu->addSeparator();
    QAction *hideAAct = settingsMenu->addAction("隐藏通道A");
    hideAAct->setCheckable(true);
    QAction *hideBAct = settingsMenu->addAction("隐藏通道B");
    hideBAct->setCheckable(true);
    QAction *hideABAct = settingsMenu->addAction("隐藏通道A-B");
    hideABAct->setCheckable(true);
    QAction *hideLogAct = settingsMenu->addAction("隐藏日志");
    hideLogAct->setCheckable(true);
    // 四个勾选项点击后菜单保持打开
    settingsMenu->setStayOpenActions({hideAAct, hideBAct, hideABAct, hideLogAct});
    settingsBtn->setMenu(settingsMenu);
    toolbar->addWidget(settingsBtn);

    // ========== 开始/停止测量 ==========
    m_startStopAction = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "开始测量");

    // ========== 复位 ==========
    QAction *resetAct = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), "复位");

    // ========== 数据处理 ==========
    QAction *dataAct = toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), "数据处理");

    // ========== TC4 加热开关 ==========
    m_ovenAction = toolbar->addAction(m_iconOvenOff, "TC4：关");
    m_ovenAction->setToolTip("点击开启/关闭 TC4 加热 (寄存器 0x0400)");
    connect(m_ovenAction, &QAction::triggered, this, &MainWindow::onOvenToggle);

    // ========== TCD 电源状态指示灯 ==========
    m_tcdPowerAction = toolbar->addAction(m_iconTcdOff, "TCD电源：未连接");
    m_tcdPowerAction->setToolTip("TCD电源状态（由 0x00A BIT1 判断）");
    updateTcdPowerIcon();
    updateOvenIcon();

    // ========== 信号槽连接 ==========
    connect(connectAct, &QAction::triggered, this, &MainWindow::openConnectionDialog);
    connect(dataAct, &QAction::triggered, this, &MainWindow::openDataProcessing);
    connect(globalSettingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    connect(m_startStopAction, &QAction::triggered, this, &MainWindow::startStopAutoProcess);
    connect(resetAct, &QAction::triggered, this, &MainWindow::resetSystem);
    connect(hideAAct, &QAction::toggled, this, [this](bool checked) {
        if (m_monitorTab) m_monitorTab->setChannelAVisible(!checked);
    });
    connect(hideBAct, &QAction::toggled, this, [this](bool checked) {
        if (m_monitorTab) m_monitorTab->setChannelBVisible(!checked);
    });
    connect(hideABAct, &QAction::toggled, this, [this](bool checked) {
        if (m_monitorTab) m_monitorTab->setChannelABVisible(!checked);
    });
    connect(hideLogAct, &QAction::toggled, this, &MainWindow::toggleLogVisible);
}


void MainWindow::onOvenToggle()
{
    if (!m_comm || !m_comm->isConnected()) {
        QString msg = "未连接，无法控制 TC4";
        m_log->appendLog("TC4", msg);
        m_log->appendFileOnly("TC4", msg);
        return;
    }

    bool newState = !m_ovenEnabled;
    quint16 expected = newState ? 1 : 0;

    // 发送写 0x0400
    m_comm->setColumnOvenEnable(newState);
    m_ovenEnabled = newState;
    updateOvenIcon();

    QString actionMsg = newState ? "发送开启 TC4 加热命令" : "发送关闭 TC4 加热命令";
    m_log->appendLog("TC4", actionMsg);
    m_log->appendFileOnly("TC4", actionMsg);

    // 延迟 1000ms 后读取 0x0400 验证
    QTimer::singleShot(1000, this, [this, newState, expected]() {
        if (!m_comm || !m_comm->isConnected()) return;

        m_comm->requestRegisterRead(0x0400, [this, newState, expected](quint16 v) {
            if (v == expected) {
                QString msg = QString("TC4 %1 验证成功: 0x0400 = %2")
                                  .arg(newState ? "开启" : "关闭").arg(v);
                m_log->appendLog("TC4", msg);
                m_log->appendFileOnly("TC4", msg);
            } else {
                QString msg = QString("TC4 %1 验证失败: 期望 %2，实际 %3")
                                  .arg(newState ? "开启" : "关闭")
                                  .arg(expected).arg(v);
                m_log->appendLog("TC4", msg);
                m_log->appendFileOnly("TC4", msg);

                // 回滚显示
                m_ovenEnabled = !newState;
                updateOvenIcon();
            }
        });
    });
}

void MainWindow::updateOvenIcon()
{
    if (!m_ovenAction) return;
    if (m_ovenEnabled) {
        m_ovenAction->setIcon(m_iconOvenOn);
        m_ovenAction->setText("TC4：开");
    } else {
        m_ovenAction->setIcon(m_iconOvenOff);
        m_ovenAction->setText("TC4：关");
    }
}

void MainWindow::createTabs()
{
    m_tabs = new QTabWidget(this);

    m_controlTab = new ControlTab;
    m_monitorTab = new MonitorTab;
    m_otherTab = new OtherTab;

    m_tabs->addTab(m_controlTab, "控制");
    m_tabs->addTab(m_monitorTab, "监视");
    m_tabs->addTab(m_otherTab, "其他");

    m_controlTab->setCommunication(m_comm);
    m_monitorTab->setCommunication(m_comm);
    m_otherTab->setCommunication(m_comm);

    // 显示长度传递
    connect(m_controlTab, &ControlTab::displayLengthChanged,
            this, [this](const QString &device, int length) {
                if (!m_otherTab) return;
                if (device == "Oven") {
                    m_otherTab->setColumnOvenLength(length);
                } else if (device == "Pressure") {
                    m_otherTab->setPressureLength(length);
                } else if (device == "TCD") {
                    m_otherTab->setTcdTempLength(length);
                } else if (device == "Flow1") {
                    m_otherTab->setFlow1Length(length);
                } else if (device == "Flow2") {
                    m_otherTab->setFlow2Length(length);
                }
            });

    // ==========================================================
    // 控制页日志 + 电磁阀/六通阀 → 触发数据保存
    //   - device 以 "NV" 开头：记录普通阀门日志
    //   - device == "电磁阀"：与六通阀同步切换，用于启停数据保存
    // ==========================================================
    connect(m_controlTab, &ControlTab::commandRequested, this,
            [this](const QString &device, bool state) {
                if (device.startsWith("NV")) {
                    QString msg = QString("阀门 %1 状态:%2")
                                      .arg(device)
                                      .arg(state ? "开启" : "关闭");
                    m_log->appendLog("控制", msg);
                    m_log->appendFileOnly("控制", msg);
                }
                else if (device == "电磁阀") {
                    if (state) {
                        // 切到采样位置 → 开始保存
                        if (m_monitorTab) m_monitorTab->startDataSaveNow();
                        m_log->appendLog("控制", "六通阀切换至采样位置，开始数据保存");
                        m_log->appendFileOnly("控制", "六通阀切换至采样位置，开始数据保存");
                    } else {
                        // 切到装填位置 → 停止保存
                        if (m_monitorTab) m_monitorTab->stopDataSave();
                        m_log->appendLog("控制", "六通阀切换至装填位置，停止数据保存");
                        m_log->appendFileOnly("控制", "六通阀切换至装填位置，停止数据保存");
                    }
                }
            });

    // 控制页通用日志（ControlTab 主动发的 logMessage）
    connect(m_controlTab, &ControlTab::logMessage, this,
            [this](const QString &type, const QString &event) {
                m_log->appendLog(type, event);
                m_log->appendFileOnly(type, event);
            });

    // 监视页日志
    connect(m_monitorTab, &MonitorTab::logMessage, this,
            [this](const QString &type, const QString &event) {
                m_log->appendLog(type, event);
                m_log->appendFileOnly(type, event);
            });

    // 其他页日志
    connect(m_otherTab, &OtherTab::logMessage, this,
            [this](const QString &type, const QString &event) {
                m_log->appendLog(type, event);
                m_log->appendFileOnly(type, event);
            });
}

void MainWindow::onCommunicationConnected()
{
    m_comm->startPolling();
    m_log->appendLog("通信", "已连接并开始轮询");
    m_log->appendFileOnly("通信", "已连接并开始轮询");

    if (m_connectAction) {
        m_connectAction->setIcon(m_iconConnectDefault);
    }

    // 重置故障位缓存，让第一次读到的 0x000A 能触发一次日志
    m_lastDiag = 0xFFFF;

    // 立即检查一次 TCD 状态
    checkTcdPowerStatus();
}

void MainWindow::onCommunicationDisconnected()
{
    m_comm->stopPolling();
    m_log->appendLog("通信", "连接断开");
    m_log->appendFileOnly("通信", "连接断开");

    if (m_connectAction) {
        QPixmap blackIcon(32, 32);
        blackIcon.fill(Qt::transparent);
        QPainter painter(&blackIcon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::black);
        painter.setPen(Qt::black);
        painter.drawEllipse(8, 8, 16, 16);
        m_connectAction->setIcon(QIcon(blackIcon));
    }

    // TCD 状态视为未连接
    m_tcdPowered = false;
    m_comm->setTcdPowered(false);
    updateTcdPowerIcon();
    if (m_monitorTab) m_monitorTab->setTcdPowered(false);

    // TC4 状态重置（未知）
    m_ovenEnabled = false;
    updateOvenIcon();
}

void MainWindow::openConnectionDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Modbus-TCP 连接设置");
    dlg.setMinimumWidth(360);

    QFormLayout form(&dlg);

    QSettings settings("MyCompany", "MicroGC");
    QString savedIp = settings.value("connection/ip", "192.168.10.8").toString();
    int savedPort = settings.value("connection/port", 502).toInt();

    QLineEdit *ipEdit = new QLineEdit(savedIp, &dlg);
    QSpinBox *portSpin = new QSpinBox(&dlg);
    portSpin->setRange(1, 65535);
    portSpin->setValue(savedPort);

    form.addRow("IP 地址:", ipEdit);
    form.addRow("端口:", portSpin);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        settings.setValue("connection/ip", ipEdit->text());
        settings.setValue("connection/port", portSpin->value());
        settings.sync();

        m_comm->connectToDevice(ipEdit->text(), portSpin->value());
        QString msg = QString("尝试连接 %1:%2").arg(ipEdit->text()).arg(portSpin->value());
        m_log->appendLog("连接", msg);
        m_log->appendFileOnly("连接", msg);
    }
}

void MainWindow::onFaultBitsChanged(quint16 diag)
{
    struct BitInfo { int bit; const char *name; };
    static const BitInfo bits[] = {
                                    {0,  "TC4通信故障"},
                                    {1,  "TCD通信故障"},
                                    {2,  "TCD命令FIFO满"},
                                    {3,  "TC4命令FIFO满"},
                                    {4,  "TCD A超量程"},
                                    {5,  "TCD B超量程"},
                                    {6,  "TCD A-B超量程"},
                                    {7,  "流量1输入超量程"},
                                    {8,  "压力输入超量程"},
                                    {9,  "上位机通信超时"},
                                    {10, "TPC6200采集故障"},
                                    {11, "网络参数无有效记录"},
                                    {12, "网络参数保存失败"},
                                    {13, "流量2输入超量程"},
                                    };

    QStringList faultList;
    for (const BitInfo &b : bits) {
        if (diag & (1 << b.bit)) {
            faultList.append(b.name);
        }
    }

    QString msg;
    if (faultList.isEmpty()) {
        msg = QString("故障状态清除: 0x%1").arg(diag, 4, 16, QChar('0'));
    } else {
        msg = QString("故障状态: 0x%1 → %2")
                  .arg(diag, 4, 16, QChar('0'))
                  .arg(faultList.join("，"));
    }

    m_log->appendLog("诊断", msg);
    m_log->appendFileOnly("诊断", msg);
}

void MainWindow::openDataProcessing()
{
    DataProcessingDialog *dlg = new DataProcessingDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MainWindow::openSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle("全局参数设置");
    dlg.setMinimumSize(1000, 720);

    // ============ 外层滚动区 ============
    auto *dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    dlgLayout->addWidget(scroll);

    auto *content = new QWidget();
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);
    scroll->setWidget(content);

    // ============================================================
    //  两列布局
    // ============================================================
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(14);

    // ---------------- 左列：仪器 / TCD / 底部按钮 ----------------
    auto *leftCol = new QVBoxLayout;
    leftCol->setSpacing(10);

    // ----- 仪器参数 -----
    auto *instrGroup = new QGroupBox("仪器参数", content);
    QSpinBox *flow1Spin = nullptr;
    QSpinBox *flow2Spin = nullptr;
    QSpinBox *ovenTempSpin = nullptr;
    QSpinBox *tcdTempSpin = nullptr;
    {
        auto *form = new QFormLayout(instrGroup);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);

        flow1Spin = new QSpinBox(instrGroup);
        flow1Spin->setRange(1, 1000);
        flow1Spin->setSuffix(" 电压");
        form->addRow("载气电压:", flow1Spin);

        flow2Spin = new QSpinBox(instrGroup);
        flow2Spin->setRange(1, 1000);
        flow2Spin->setSuffix(" 电压");
        form->addRow("参比气电压:", flow2Spin);

        ovenTempSpin = new QSpinBox(instrGroup);
        ovenTempSpin->setRange(1, 1000);
        ovenTempSpin->setSuffix(" ℃");
        form->addRow("柱温箱温度:", ovenTempSpin);

        tcdTempSpin = new QSpinBox(instrGroup);
        tcdTempSpin->setRange(0, 500);
        tcdTempSpin->setSuffix(" ℃");
        tcdTempSpin->setToolTip("TS 98\r  设置TCD温度");
        form->addRow("TCD温度:", tcdTempSpin);
    }
    leftCol->addWidget(instrGroup);

    // ----- TCD 参数 -----
    auto *tcdGroup = new QGroupBox("TCD 参数", content);
    QSpinBox *collectPointsSpin = nullptr;
    QSpinBox *averageSpin = nullptr;
    QComboBox *precisionCombo = nullptr;
    QDoubleSpinBox *levelASpin = nullptr;
    QDoubleSpinBox *levelBSpin = nullptr;
    QDoubleSpinBox *levelABSpin = nullptr;
    QPushButton *setCollectBtn = nullptr;
    QPushButton *setAverageBtn = nullptr;
    {
        auto *form = new QFormLayout(tcdGroup);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);

        collectPointsSpin = new QSpinBox(tcdGroup);
        collectPointsSpin->setRange(100, 100000);
        setCollectBtn = new QPushButton("设置", tcdGroup);
        auto *collectRow = new QHBoxLayout;
        collectRow->setContentsMargins(0, 0, 0, 0);
        collectRow->addWidget(collectPointsSpin);
        collectRow->addWidget(setCollectBtn);
        form->addRow("采集点数:", collectRow);

        averageSpin = new QSpinBox(tcdGroup);
        averageSpin->setRange(1, 100);
        setAverageBtn = new QPushButton("设置", tcdGroup);
        auto *averageRow = new QHBoxLayout;
        averageRow->setContentsMargins(0, 0, 0, 0);
        averageRow->addWidget(averageSpin);
        averageRow->addWidget(setAverageBtn);
        form->addRow("均点设置:", averageRow);

        precisionCombo = new QComboBox(tcdGroup);
        precisionCombo->addItem("0.001", 1);
        precisionCombo->addItem("0.01", 10);
        precisionCombo->addItem("0.05", 50);
        precisionCombo->addItem("0.1", 100);
        precisionCombo->addItem("1", 1000);
        precisionCombo->setToolTip("PF 0.05\r   设置最小精度[]");
        form->addRow("最小精度:", precisionCombo);

        levelASpin = new QDoubleSpinBox(tcdGroup);
        levelASpin->setRange(-1.2, 1.2);
        levelASpin->setDecimals(1);
        levelASpin->setSingleStep(0.1);
        levelASpin->setSuffix(" V");
        levelASpin->setToolTip("ZA 0.1\r");
        form->addRow("A电平:", levelASpin);

        levelBSpin = new QDoubleSpinBox(tcdGroup);
        levelBSpin->setRange(-1.2, 1.2);
        levelBSpin->setDecimals(1);
        levelBSpin->setSingleStep(0.1);
        levelBSpin->setSuffix(" V");
        levelBSpin->setToolTip("ZB 0.1\r");
        form->addRow("B电平:", levelBSpin);

        levelABSpin = new QDoubleSpinBox(tcdGroup);
        levelABSpin->setRange(-1.2, 1.2);
        levelABSpin->setDecimals(1);
        levelABSpin->setSingleStep(0.1);
        levelABSpin->setSuffix(" V");
        levelABSpin->setToolTip("ZR 0.1\r");
        form->addRow("AB电平:", levelABSpin);
    }
    leftCol->addWidget(tcdGroup);

    // ----- 底部按钮（导出 / 导入 / 应用 / 关闭）-----
    auto *leftBtnBar = new QHBoxLayout;
    QPushButton *saveConfigBtn = new QPushButton("导出配置", content);
    QPushButton *loadConfigBtn = new QPushButton("导入配置", content);
    QPushButton *applyBtn      = new QPushButton("应用",     content);
    QPushButton *closeBtn      = new QPushButton("关闭",     content);
    leftBtnBar->addWidget(saveConfigBtn);
    leftBtnBar->addWidget(loadConfigBtn);
    leftBtnBar->addStretch();
    leftBtnBar->addWidget(applyBtn);
    leftBtnBar->addWidget(closeBtn);
    leftCol->addLayout(leftBtnBar);

    leftCol->addStretch();

    // ---------------- 右列：自动流程 / 自动流程配置 ----------------
    auto *rightCol = new QVBoxLayout;
    rightCol->setSpacing(10);

    // ----- 自动流程 -----
    auto *autoProcessGroup = new QGroupBox("自动流程", content);
    auto *autoProcessLayout = new QVBoxLayout(autoProcessGroup);
    autoProcessLayout->setSpacing(8);

    auto *btnBar = new QHBoxLayout;
    QPushButton *btnAddCollect  = new QPushButton("+ 数据采集", autoProcessGroup);
    QPushButton *btnAddValve    = new QPushButton("+ 六通阀", autoProcessGroup);
    QPushButton *btnAddPolarity = new QPushButton("+ 极性反转", autoProcessGroup);
    QPushButton *btnRemove      = new QPushButton("- 删除选中", autoProcessGroup);
    btnBar->addWidget(btnAddCollect);
    btnBar->addWidget(btnAddValve);
    btnBar->addWidget(btnAddPolarity);
    btnBar->addWidget(btnRemove);
    btnBar->addStretch();
    autoProcessLayout->addLayout(btnBar);

    QTableWidget *eventTable = new QTableWidget(autoProcessGroup);
    eventTable->setColumnCount(4);
    eventTable->setHorizontalHeaderLabels({"类型", "开时间(min)", "关时间(min)", "操作"});
    eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    eventTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    eventTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    eventTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    eventTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    eventTable->setAlternatingRowColors(true);
    eventTable->verticalHeader()->setDefaultSectionSize(34);
    eventTable->setMinimumHeight(150);
    autoProcessLayout->addWidget(eventTable, 1);

    auto *timelineLabel = new QLabel("时间轴总览：", autoProcessGroup);
    autoProcessLayout->addWidget(timelineLabel);
    auto *timeline = new TimelineWidget(autoProcessGroup);
    timeline->setMinimumHeight(120);
    autoProcessLayout->addWidget(timeline);

    rightCol->addWidget(autoProcessGroup, 1);

    // ----- 自动流程配置 -----
    auto *autoConfigGroup = new QGroupBox("自动流程配置", content);
    QLineEdit *autoPathEdit = nullptr;
    QLineEdit *autoNameEdit = nullptr;
    QSpinBox  *cycleSpin    = nullptr;
    {
        auto *form = new QFormLayout(autoConfigGroup);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(10);

        autoPathEdit = new QLineEdit(autoConfigGroup);
        form->addRow("保存路径:", autoPathEdit);

        autoNameEdit = new QLineEdit(autoConfigGroup);
        form->addRow("文件名前缀:", autoNameEdit);

        cycleSpin = new QSpinBox(autoConfigGroup);
        cycleSpin->setRange(1, 100);
        cycleSpin->setSuffix(" 次");
        form->addRow("测量次数:", cycleSpin);
    }
    rightCol->addWidget(autoConfigGroup, 0);

    // ---------------- 组装 ----------------
    topRow->addLayout(leftCol, 1);
    topRow->addLayout(rightCol, 1);
    mainLayout->addLayout(topRow, 1);

    // ============================================================
    //  事件表格刷新
    // ============================================================
    bool refreshing = false;
    std::function<void()> refreshTable = [&]() {
        refreshing = true;
        auto groups = m_autoProcess->groups();
        eventTable->setRowCount(groups.size());
        for (int i = 0; i < groups.size(); ++i) {
            const auto &g = groups[i];

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
            auto *typeLabel = new QLabel(typeStr, eventTable);
            QFont f = typeLabel->font();
            f.setBold(true);
            typeLabel->setFont(f);
            QPalette pal = typeLabel->palette();
            pal.setColor(QPalette::WindowText, color);
            typeLabel->setPalette(pal);
            typeLabel->setAlignment(Qt::AlignCenter);
            eventTable->setCellWidget(i, 0, typeLabel);

            auto *onSpin = new QDoubleSpinBox(eventTable);
            onSpin->setRange(0.0, 999.0);
            onSpin->setDecimals(3);
            onSpin->setSingleStep(0.01);
            onSpin->setValue(g.onTimeMin);
            onSpin->setSuffix(" min");
            eventTable->setCellWidget(i, 1, onSpin);

            auto *offSpin = new QDoubleSpinBox(eventTable);
            offSpin->setRange(0.0, 999.0);
            offSpin->setDecimals(3);
            offSpin->setSingleStep(0.01);
            offSpin->setValue(g.offTimeMin);
            offSpin->setSuffix(" min");
            eventTable->setCellWidget(i, 2, offSpin);

            connect(onSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    eventTable, [&, i](double v) {
                        if (refreshing) return;
                        auto list = m_autoProcess->groups();
                        if (i >= 0 && i < list.size()) {
                            list[i].onTimeMin = v;
                            m_autoProcess->setGroups(list);   // ← 实时保存
                            timeline->setGroups(list);
                        }
                    });
            connect(offSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    eventTable, [&, i](double v) {
                        if (refreshing) return;
                        auto list = m_autoProcess->groups();
                        if (i >= 0 && i < list.size()) {
                            list[i].offTimeMin = v;
                            m_autoProcess->setGroups(list);   // ← 实时保存
                            timeline->setGroups(list);
                        }
                    });

            auto *delBtn = new QPushButton("删除", eventTable);
            eventTable->setCellWidget(i, 3, delBtn);
            connect(delBtn, &QPushButton::clicked, eventTable, [&, i]() {
                auto list = m_autoProcess->groups();
                if (i >= 0 && i < list.size()) {
                    if (list.size() <= 1) {
                        QMessageBox::information(&dlg, "提示", "至少需要保留一组事件");
                        return;
                    }
                    list.removeAt(i);
                    m_autoProcess->setGroups(list);   // ← 实时保存
                    refreshTable();
                }
            });
        }
        timeline->setGroups(groups);
        refreshing = false;
    };

    // 增删按钮
    connect(btnAddCollect, &QPushButton::clicked, &dlg, [&]() {
        auto list = m_autoProcess->groups();
        double maxT = 0.0;
        for (const auto &g : list) maxT = qMax(maxT, g.offTimeMin);
        AutoEventGroup g;
        g.type = AutoEventType::Collect;
        g.onTimeMin = list.isEmpty() ? 0.01 : (maxT + 0.1);
        g.offTimeMin = g.onTimeMin + 1.0;
        list.append(g);
        m_autoProcess->setGroups(list);   // ← 实时保存
        refreshTable();
    });
    connect(btnAddValve, &QPushButton::clicked, &dlg, [&]() {
        auto list = m_autoProcess->groups();
        double maxT = 0.0;
        for (const auto &g : list) maxT = qMax(maxT, g.offTimeMin);
        AutoEventGroup g;
        g.type = AutoEventType::Valve;
        g.onTimeMin = maxT + 0.1;
        g.offTimeMin = g.onTimeMin + 1.0;
        list.append(g);
        m_autoProcess->setGroups(list);   // ← 实时保存
        refreshTable();
    });
    connect(btnAddPolarity, &QPushButton::clicked, &dlg, [&]() {
        auto list = m_autoProcess->groups();
        double maxT = 0.0;
        for (const auto &g : list) maxT = qMax(maxT, g.offTimeMin);
        AutoEventGroup g;
        g.type = AutoEventType::Polarity;
        g.onTimeMin = maxT + 0.1;
        g.offTimeMin = g.onTimeMin + 1.0;
        list.append(g);
        m_autoProcess->setGroups(list);   // ← 实时保存
        refreshTable();
    });
    connect(btnRemove, &QPushButton::clicked, &dlg, [&]() {
        auto selected = eventTable->selectionModel()->selectedRows();
        if (selected.isEmpty()) {
            QMessageBox::information(&dlg, "提示", "请先选中要删除的行");
            return;
        }
        auto list = m_autoProcess->groups();
        if (list.size() - selected.size() < 1) {
            QMessageBox::information(&dlg, "提示", "至少需要保留一组事件");
            return;
        }
        QList<int> rows;
        for (const auto &idx : selected) rows.append(idx.row());
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows) {
            if (r >= 0 && r < list.size()) list.removeAt(r);
        }
        m_autoProcess->setGroups(list);   // ← 实时保存
        refreshTable();
    });

    refreshTable();

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    // ============================================================
    //  读取 QSettings 初始值
    // ============================================================
    QSettings settings("MyCompany", "MicroGC");
    flow1Spin->setValue(settings.value("global/flow1Voltage", 5).toInt());
    flow2Spin->setValue(settings.value("global/flow2Voltage", 5).toInt());
    ovenTempSpin->setValue(settings.value("global/ovenTemp", 60).toInt());
    tcdTempSpin->setValue(settings.value("global/tcdTemp", 25).toInt());
    collectPointsSpin->setValue(settings.value("global/collectPoints", 1000).toInt());
    averageSpin->setValue(settings.value("global/averagePoints", 5).toInt());
    if (m_monitorTab) {
        m_monitorTab->setCollectPoints(collectPointsSpin->value());
        m_monitorTab->setAveragePoints(averageSpin->value());
    }
    int prec = settings.value("global/precision", 1).toInt();
    int pid = precisionCombo->findData(prec);
    if (pid >= 0) precisionCombo->setCurrentIndex(pid);
    levelASpin->setValue(settings.value("global/levelA", 0).toInt() / 10.0);
    levelBSpin->setValue(settings.value("global/levelB", 0).toInt() / 10.0);
    levelABSpin->setValue(settings.value("global/levelAB", 0).toInt() / 10.0);

    autoPathEdit->setText(settings.value("auto/savePath", "./auto_data").toString());
    autoNameEdit->setText(settings.value("auto/filePrefix", "auto_measure").toString());
    cycleSpin->setValue(settings.value("auto/cycleCount", 1).toInt());

    // ============================================================
    //  自动流程配置：实时保存（读取初始值后再连接，避免触发多余写入）
    // ============================================================
    connect(autoPathEdit, &QLineEdit::textChanged, &dlg, [](const QString &text) {
        QSettings s("MyCompany", "MicroGC");
        s.setValue("auto/savePath", text);
        s.sync();
    });
    connect(autoNameEdit, &QLineEdit::textChanged, &dlg, [](const QString &text) {
        QSettings s("MyCompany", "MicroGC");
        s.setValue("auto/filePrefix", text);
        s.sync();
    });
    connect(cycleSpin, qOverload<int>(&QSpinBox::valueChanged), &dlg, [](int val) {
        QSettings s("MyCompany", "MicroGC");
        s.setValue("auto/cycleCount", val);
        s.sync();
    });

    // ============================================================
    //  采集点数 / 均点数 的"设置"按钮
    // ============================================================
    connect(setCollectBtn, &QPushButton::clicked, this, [this, collectPointsSpin]() {
        if (m_monitorTab) m_monitorTab->setCollectPoints(collectPointsSpin->value());
        m_log->appendLog("设置", QString("采集点数设置为 %1").arg(collectPointsSpin->value()));
    });
    connect(setAverageBtn, &QPushButton::clicked, this, [this, averageSpin]() {
        if (m_monitorTab) m_monitorTab->setAveragePoints(averageSpin->value());
        m_log->appendLog("设置", QString("均点设置设置为 %1").arg(averageSpin->value()));
    });

    // ============================================================
    //  导出配置
    // ============================================================
    connect(saveConfigBtn, &QPushButton::clicked, &dlg, [&]() {
        QString fileName = QFileDialog::getSaveFileName(&dlg, "保存全局配置", QString(), "配置文件 (*.json)");
        if (fileName.isEmpty()) return;

        QJsonObject root;
        root["version"] = 1;

        QJsonObject instrObj;
        instrObj["flow1Voltage"] = flow1Spin->value();
        instrObj["flow2Voltage"] = flow2Spin->value();
        instrObj["ovenTemp"]     = ovenTempSpin->value();
        instrObj["tcdTemp"]      = tcdTempSpin->value();
        root["instrument"] = instrObj;

        QJsonObject tcdObj;
        tcdObj["collectPoints"] = collectPointsSpin->value();
        tcdObj["averagePoints"] = averageSpin->value();
        tcdObj["precision"]     = precisionCombo->currentData().toInt();
        tcdObj["levelA"]        = qRound(levelASpin->value() * 10.0);
        tcdObj["levelB"]        = qRound(levelBSpin->value() * 10.0);
        tcdObj["levelAB"]       = qRound(levelABSpin->value() * 10.0);
        root["tcd"] = tcdObj;

        QJsonObject autoObj;
        autoObj["savePath"]   = autoPathEdit->text();
        autoObj["filePrefix"] = autoNameEdit->text();
        autoObj["cycleCount"] = cycleSpin->value();
        root["autoMeasure"] = autoObj;

        QJsonArray groupsArr;
        auto groups = m_autoProcess->groups();
        for (const auto &g : groups) {
            QJsonObject gObj;
            gObj["type"] = static_cast<int>(g.type);
            gObj["onTimeMin"] = g.onTimeMin;
            gObj["offTimeMin"] = g.offTimeMin;
            groupsArr.append(gObj);
        }
        root["autoGroups"] = groupsArr;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(&dlg, "保存失败", "无法写入配置文件");
            return;
        }
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        QMessageBox::information(&dlg, "保存成功", "全局配置已导出到:\n" + fileName);
    });

    // ============================================================
    //  导入配置
    // ============================================================
    connect(loadConfigBtn, &QPushButton::clicked, &dlg, [&]() {
        QString fileName = QFileDialog::getOpenFileName(&dlg, "加载全局配置", QString(), "配置文件 (*.json)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(&dlg, "加载失败", "无法读取配置文件");
            return;
        }
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            QMessageBox::warning(&dlg, "加载失败", "配置文件格式错误");
            return;
        }
        QJsonObject root = doc.object();

        QJsonObject instrObj = root["instrument"].toObject();
        flow1Spin->setValue(instrObj["flow1Voltage"].toInt());
        flow2Spin->setValue(instrObj["flow2Voltage"].toInt());
        ovenTempSpin->setValue(instrObj["ovenTemp"].toInt());
        tcdTempSpin->setValue(instrObj["tcdTemp"].toInt());

        QJsonObject tcdObj = root["tcd"].toObject();
        collectPointsSpin->setValue(tcdObj["collectPoints"].toInt());
        averageSpin->setValue(tcdObj["averagePoints"].toInt());
        int precVal = tcdObj["precision"].toInt();
        int idx = precisionCombo->findData(precVal);
        if (idx >= 0) precisionCombo->setCurrentIndex(idx);
        levelASpin->setValue(tcdObj["levelA"].toInt() / 10.0);
        levelBSpin->setValue(tcdObj["levelB"].toInt() / 10.0);
        levelABSpin->setValue(tcdObj["levelAB"].toInt() / 10.0);

        QJsonObject autoObj = root["autoMeasure"].toObject();
        autoPathEdit->setText(autoObj["savePath"].toString());
        autoNameEdit->setText(autoObj["filePrefix"].toString());
        cycleSpin->setValue(autoObj["cycleCount"].toInt());

        QJsonArray groupsArr = root["autoGroups"].toArray();
        QList<AutoEventGroup> newGroups;
        for (const auto &v : groupsArr) {
            QJsonObject gObj = v.toObject();
            AutoEventGroup g;
            g.type = static_cast<AutoEventType>(gObj["type"].toInt());
            g.onTimeMin = gObj["onTimeMin"].toDouble();
            g.offTimeMin = gObj["offTimeMin"].toDouble();
            newGroups.append(g);
        }
        m_autoProcess->setGroups(newGroups);
        refreshTable();

        QMessageBox::information(&dlg, "加载成功", "配置已加载，点击「应用」生效");
    });

    // ============================================================
    //  应用：只作用于仪器参数与 TCD 参数
    //    自动流程 / 自动流程配置 → 均实时保存，不在此触发
    // ============================================================
    connect(applyBtn, &QPushButton::clicked, &dlg, [&]() {
        // 只写仪器 + TCD 相关键
        settings.setValue("global/flow1Voltage", flow1Spin->value());
        settings.setValue("global/flow2Voltage", flow2Spin->value());
        settings.setValue("global/ovenTemp", ovenTempSpin->value());
        settings.setValue("global/tcdTemp", tcdTempSpin->value());
        settings.setValue("global/collectPoints", collectPointsSpin->value());
        settings.setValue("global/averagePoints", averageSpin->value());
        settings.setValue("global/precision", precisionCombo->currentData().toInt());
        settings.setValue("global/levelA", qRound(levelASpin->value() * 10.0));
        settings.setValue("global/levelB", qRound(levelBSpin->value() * 10.0));
        settings.setValue("global/levelAB", qRound(levelABSpin->value() * 10.0));
        settings.sync();

        if (m_monitorTab) {
            m_monitorTab->setParameter("temperature", tcdTempSpin->value());
            m_monitorTab->setParameter("levelA", qRound(levelASpin->value() * 10.0));
            m_monitorTab->setParameter("levelB", qRound(levelBSpin->value() * 10.0));
            m_monitorTab->setParameter("levelAB", qRound(levelABSpin->value() * 10.0));
            m_monitorTab->setParameter("precision", precisionCombo->currentData().toInt());
            m_monitorTab->setCollectPoints(collectPointsSpin->value());
            m_monitorTab->setAveragePoints(averageSpin->value());
            m_monitorTab->applyGlobalParameters();
        }

        int flow1Val = flow1Spin->value();
        int flow2Val = flow2Spin->value();
        int ovenTemp = ovenTempSpin->value();
        QTimer::singleShot(1500, this, [this, flow1Val, flow2Val, ovenTemp]() {
            if (!m_comm) return;
            QVector<quint16> flowVals = {
                static_cast<quint16>(flow1Val),
                static_cast<quint16>(flow2Val)
            };
            m_comm->writeMultipleRegisters(0x0402, flowVals, "全局设置: 载气+参比气电压");
            m_comm->requestRegisterRead(0x0400, [this, ovenTemp](quint16) {
                QVector<quint16> ovenVals = {
                    static_cast<quint16>(ovenTemp),
                    1
                };
                m_comm->writeMultipleRegisters(0x03FF, ovenVals, "全局设置: 柱温箱温度+使能");
            });
        });

        m_log->appendLog("设置", "仪器参数与 TCD 参数已应用并保存");
        m_log->appendFileOnly("设置", "仪器参数与 TCD 参数已应用并保存");
    });

    dlg.exec();
}

void MainWindow::startStopAutoProcess()
{
    if (!m_autoProcess) return;
    if (m_autoProcess->isRunning()) {
        m_autoProcess->stop();
    } else {
        m_autoProcess->start();
    }
}

void MainWindow::resetSystem()
{
    // 复位：如果自动流程正在运行，先停止；再由 MonitorTab 发送关闭命令
    if (m_autoProcess && m_autoProcess->isRunning()) {
        m_autoProcess->stop();
    }

    // 停止数据保存（如果正在记录）
    if (m_monitorTab) {
        m_monitorTab->stopDataSave();
        m_monitorTab->stopPolarityReversal();
    }

    // 关闭所有硬件
    if (m_comm && m_comm->isConnected()) {
        m_comm->setColumnOvenEnable(false);
        m_comm->setValveBit(0, false);
        m_comm->setSixWayValve1(false);
        m_comm->setFlow1Voltage(0);
        m_comm->setFlow2Voltage(0);

        QString msg = "系统复位：已发送关闭命令（TC4/电磁阀/六通阀/流量电压）";
        m_log->appendLog("复位", msg);
        m_log->appendFileOnly("复位", msg);
    }

    // TC4 状态重置
    m_ovenEnabled = false;
    updateOvenIcon();
}

void MainWindow::toggleLogVisible(bool visible)
{
    if (m_log) m_log->setVisible(!visible);
}

void MainWindow::checkTcdPowerStatus()
{
    if (!m_comm || !m_comm->isConnected()) {
        m_tcdPowered = false;
        m_comm->setTcdPowered(false);
        updateTcdPowerIcon();
        if (m_monitorTab) m_monitorTab->setTcdPowered(false);
        return;
    }

    // 读缓存的 0x000A（慢速轮询每秒更新）
    quint16 diag = m_comm->diagRegister();

    bool tc4Fail = (diag & (1 << 0)) != 0;   // BIT0
    bool tcdFail = (diag & (1 << 1)) != 0;   // BIT1
    bool powered = !(tc4Fail || tcdFail);

    if (m_tcdPowered != powered) {
        m_tcdPowered = powered;
        m_comm->setTcdPowered(powered);
        updateTcdPowerIcon();
        if (m_monitorTab) m_monitorTab->setTcdPowered(powered);
    }
}

void MainWindow::updateTcdPowerIcon()
{
    if (!m_tcdPowerAction) return;
    if (m_tcdPowered) {
        m_tcdPowerAction->setIcon(m_iconTcdOn);
        m_tcdPowerAction->setText("TCD电源：已通电");
    } else {
        m_tcdPowerAction->setIcon(m_iconTcdOff);
        m_tcdPowerAction->setText("TCD电源：未连接");
    }
}