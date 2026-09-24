#include "mainwindow.h"
#include "logwidget.h"
#include "communication.h"
#include "controltab.h"
#include "monitortab.h"
#include "othertab.h"
#include "dataprocessing.h"

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
    , m_tcdReplyReceived(false)
    , m_checkActive(false)
{
    setWindowTitle("MicroGC");
    resize(1200, 900);

    m_comm = new Communication(this);
    m_log = new LogWidget(this);

    // ========== TCD 电源状态图标 ==========
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
    painterOn.setBrush(QColor(0, 255, 0));
    painterOn.setPen(QPen(Qt::black, 2));
    painterOn.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
    painterOn.setBrush(Qt::white);
    painterOn.setPen(Qt::NoPen);
    painterOn.drawEllipse(8, 6, 5, 5);
    m_iconTcdOn = QIcon(onPixmap);

    // ========== 柱温箱开关图标 ==========
    {
        const int iconSize = 24;

        // 关闭状态：深灰圆 + 蓝色边框
        QPixmap offPixmap(iconSize, iconSize);
        offPixmap.fill(Qt::transparent);
        QPainter painterOff(&offPixmap);
        painterOff.setRenderHint(QPainter::Antialiasing);
        painterOff.setBrush(QColor(120, 120, 120));
        painterOff.setPen(QPen(QColor(70, 70, 180), 2));
        painterOff.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
        m_iconOvenOff = QIcon(offPixmap);

        // 开启状态：橙色圆 + 白边框 + 高光
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

    // ========== TCD 电源状态定时器（每 20 秒） ==========
    m_tcdCheckTimer = new QTimer(this);
    m_tcdCheckTimer->setInterval(2000);
    m_tcdCheckTimer->setTimerType(Qt::PreciseTimer);
    connect(m_tcdCheckTimer, &QTimer::timeout, this, &MainWindow::checkTcdPowerStatus);

    // ========== 自动流程管理器 ==========
    // ========== 自动流程日志：界面 + 文件 ==========
    m_autoProcess = new AutoProcessManager(m_comm, this);
    connect(m_autoProcess, &AutoProcessManager::logMessage, this,
            [this](const QString &type, const QString &event) {
                m_log->appendLog(type, event);        // 上界面
                m_log->appendFileOnly(type, event);   // 写文件
            });

    // 自动流程状态变化：界面 + 文件
    connect(m_autoProcess, &AutoProcessManager::stateChanged, this,
            [this](const QString &state) {
                QString msg = "状态: " + state;
                m_log->appendLog("自动流程", msg);
                m_log->appendFileOnly("自动流程", msg);

                if (m_startStopAction) {
                    if (state == "空闲") {
                        m_startStopAction->setText("开始自动流程");
                        m_startStopAction->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
                    } else {
                        m_startStopAction->setText("停止");
                        m_startStopAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
                    }
                }
            });

    // ========== 报文日志：界面 + 文件 ==========
    // FastPoll/SlowPoll 已在 sendRequest 中静默，不会 emit logPacket，
    // 所以这里只会收到：写命令、按需读取（Read/ReadRange）的报文，
    // 全部上界面 + 写文件，不会刷屏。
    connect(m_comm, &Communication::logPacket, this,
            [this](const QString &direction, const QString &dataHex) {
                QString msg = QString("[%1] %2").arg(direction, dataHex);
                m_log->appendLog("通信", msg);        // 上界面
                m_log->appendFileOnly("通信", msg);   // 写文件
            });

    // ========== 通信状态消息：界面 + 文件 ==========
    connect(m_comm, &Communication::statusMessage, this, [this](const QString &msg) {
        m_log->appendLog("通信", msg);        // 上界面
        m_log->appendFileOnly("通信", msg);   // 写文件
    });

    connect(m_comm, &Communication::connected, this, &MainWindow::onCommunicationConnected);
    connect(m_comm, &Communication::disconnected, this, &MainWindow::onCommunicationDisconnected);

    connect(m_autoProcess, &AutoProcessManager::deviceStateChanged,
            this, [this](const QString &device, bool state) {
                if (device == "六通阀" || device == "电磁阀") {
                    if (m_controlTab) m_controlTab->updateDeviceState(device, state);
                }
            });

    createActions();
    createTabs();

    // ================= 启动时从 QSettings 加载采集点数和均点设置 =================
    if (m_monitorTab) {
        QSettings settings("MyCompany", "MicroGC");
        int collectPoints = settings.value("global/collectPoints", 1000).toInt();
        int averagePoints = settings.value("global/averagePoints", 5).toInt();
        m_monitorTab->setCollectPoints(collectPoints);
        m_monitorTab->setAveragePoints(averagePoints);
    }

    m_tcdCheckTimer->start();

    connect(m_autoProcess, &AutoProcessManager::saveDataTriggered,
            m_monitorTab, &MonitorTab::startAutoSave);
    connect(m_autoProcess, &AutoProcessManager::stopDataSaveTriggered,
            m_monitorTab, &MonitorTab::stopDataSave);
    connect(m_comm, &Communication::faultBitsChanged,
            this, &MainWindow::onFaultBitsChanged);

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_log, 0);
    setCentralWidget(central);
}

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
            // 手动触发 action，不调用基类，菜单保持打开
            action->trigger();
            return;
        }
        QMenu::mouseReleaseEvent(e);
    }

private:
    QSet<QAction*> m_stayOpen;
};

void MainWindow::createActions()
{
    QToolBar *toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setIconSize(QSize(24, 24));

    QAction *connectAct = toolbar->addAction(style()->standardIcon(QStyle::SP_DriveNetIcon), "连接");
    m_connectAction = connectAct;
    m_iconConnectDefault = connectAct->icon();

    QAction *dataAct = toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), "数据处理");

    QToolButton *settingsBtn = new QToolButton(this);
    settingsBtn->setText("设置");
    settingsBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
    settingsBtn->setPopupMode(QToolButton::InstantPopup);
    settingsBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    StayOpenMenu *settingsMenu = new StayOpenMenu(settingsBtn);
    QAction *globalSettingsAct = settingsMenu->addAction("全局参数设置");
    QAction *autoSettingsAct = settingsMenu->addAction("自动流程设置");
    settingsMenu->addSeparator();
    QAction *hideAAct = settingsMenu->addAction("隐藏通道A");
    hideAAct->setCheckable(true);
    QAction *hideBAct = settingsMenu->addAction("隐藏通道B");
    hideBAct->setCheckable(true);
    QAction *hideABAct = settingsMenu->addAction("隐藏通道A-B");
    hideABAct->setCheckable(true);
    QAction *hideLogAct = settingsMenu->addAction("隐藏日志");
    hideLogAct->setCheckable(true);

    // 四个勾选项点击后菜单保持打开，可连续勾选
    settingsMenu->setStayOpenActions({hideAAct, hideBAct, hideABAct, hideLogAct});

    settingsBtn->setMenu(settingsMenu);
    toolbar->addWidget(settingsBtn);

    m_startStopAction = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "开始自动流程");
    // ========== 复位按钮 ==========
    QAction *resetAct = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), "复位");

    // ========== 柱温箱开关按钮 ==========
    m_ovenAction = toolbar->addAction(m_iconOvenOff, "柱温箱：关");
    m_ovenAction->setToolTip("点击开启/关闭柱温箱 (寄存器 0x0400)");
    connect(m_ovenAction, &QAction::triggered, this, &MainWindow::onOvenToggle);

    // ========== TCD 电源状态指示灯（不可点击，仅显示状态） ==========
    m_tcdPowerAction = toolbar->addAction(m_iconTcdOff, "TCD电源：未连接");
    m_tcdPowerAction->setToolTip("TCD电源状态");

    updateTcdPowerIcon();

    connect(connectAct, &QAction::triggered, this, &MainWindow::openConnectionDialog);
    connect(dataAct, &QAction::triggered, this, &MainWindow::openDataProcessing);
    connect(globalSettingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    connect(autoSettingsAct, &QAction::triggered, this, &MainWindow::openAutoProcessSettings);
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
        QString msg = "未连接，无法控制柱温箱";
        m_log->appendLog("柱温箱", msg);
        m_log->appendFileOnly("柱温箱", msg);
        return;
    }

    bool newState = !m_ovenEnabled;
    quint16 expected = newState ? 1 : 0;

    // 发送写命令 0x0400
    m_comm->setColumnOvenEnable(newState);
    m_ovenEnabled = newState;
    updateOvenIcon();

    QString actionMsg = newState ? "发送开启柱温箱命令" : "发送关闭柱温箱命令";
    m_log->appendLog("柱温箱", actionMsg);
    m_log->appendFileOnly("柱温箱", actionMsg);

    // 500ms 后读取 0x0400 验证
    QTimer::singleShot(500, this, [this, newState, expected]() {
        if (!m_comm || !m_comm->isConnected()) return;

        m_comm->requestRegisterRead(0x0400, [this, newState, expected](quint16 v) {
            if (v == expected) {
                QString msg = QString("柱温箱%1验证成功: 0x0400 = %2")
                                  .arg(newState ? "开启" : "关闭").arg(v);
                m_log->appendLog("柱温箱", msg);
                m_log->appendFileOnly("柱温箱", msg);
            } else {
                QString msg = QString("柱温箱%1验证失败: 期望 %2，实际 %3")
                                  .arg(newState ? "开启" : "关闭")
                                  .arg(expected).arg(v);
                m_log->appendLog("柱温箱", msg);
                m_log->appendFileOnly("柱温箱", msg);

                // 验证失败，恢复显示状态
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
        m_ovenAction->setText("柱温箱：开");
    } else {
        m_ovenAction->setIcon(m_iconOvenOff);
        m_ovenAction->setText("柱温箱：关");
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

    connect(m_controlTab, &ControlTab::commandRequested, this, [this](const QString &device, bool state) {
        if (device.startsWith("NV")) {
            m_log->appendLog("控制", QString("阀门 %1 状态:%2").arg(device).arg(state ? "开启" : "关闭"));
        }
    });
    connect(m_controlTab, &ControlTab::logMessage, this, [this](const QString &type, const QString &event) {
        m_log->appendLog(type, event);
        m_log->appendFileOnly(type, event);
    });
    connect(m_controlTab, &ControlTab::sixWayValveToggled, this, [this]() {
        if (m_monitorTab) m_monitorTab->startAutoSave();
    });

    connect(m_monitorTab, &MonitorTab::logMessage, this, [this](const QString &type, const QString &event) {
        m_log->appendLog(type, event);
        m_log->appendFileOnly(type, event);
    });
    connect(m_otherTab, &OtherTab::logMessage, this, [this](const QString &type, const QString &event) {
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

    checkTcdPowerStatus();
}

void MainWindow::onCommunicationDisconnected()
{
    m_comm->stopPolling();
    m_log->appendLog("通信", "连接断开");
    m_log->appendFileOnly("通信", "连接断开");

    // 连接断开时，将连接按钮图标设为黑色
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

    // 断开连接后，TCD 电源视为关闭
    m_tcdPowered = false;
    m_comm->setTcdPowered(false);
    updateTcdPowerIcon();

    // 通知 MonitorTab 停止记录时间轴（避免断开后继续累积）
    if (m_monitorTab) {
        m_monitorTab->setTcdPowered(false);
    }
}

void MainWindow::openConnectionDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Modbus-TCP 连接设置");
    dlg.setMinimumWidth(360);

    QFormLayout form(&dlg);

    // 从 QSettings 读取上次保存的 IP 和端口
    QSettings settings("MyCompany", "MicroGC");
    QString savedIp = settings.value("connection/ip", "192.168.10.100").toString();
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
        // 保存本次连接设置到 QSettings
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
    struct BitInfo {
        int bit;
        const char *name;
    };
    static const BitInfo bits[] = {
                                    {0,  "TC4通信故障"},
                                    {1,  "TCD通信故障"},
                                    {2,  "TCD命令FIFO满"},
                                    {3,  "TC4命令FIFO满"},
                                    {4,  "TCD A超量程"},
                                    {5,  "TCD B超量程"},
                                    {6,  "TCD A-B超量程"},
                                    {7,  "流量输入过量程"},
                                    {8,  "压力输入过量程"},
                                    {9,  "上位机通信超时"},
                                    {10, "TPC6200采集故障"},
                                    {11, "网络参数无有效记录"},
                                    {12, "网络参数保存失败"},
                                    };

    // 收集当前置位的故障
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
    dlg.setMinimumWidth(520);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    // ================= 仪器参数 =================
    QGroupBox *instrGroup = new QGroupBox("仪器参数", &dlg);
    QFormLayout *instrForm = new QFormLayout(instrGroup);

    QSpinBox *flow1Spin = new QSpinBox(instrGroup);
    flow1Spin->setRange(1, 1000);
    flow1Spin->setSuffix(" 电压");
    instrForm->addRow("流量器A电压:", flow1Spin);

    QSpinBox *flow2Spin = new QSpinBox(instrGroup);
    flow2Spin->setRange(1, 1000);
    flow2Spin->setSuffix(" 电压");
    instrForm->addRow("流量器B电压:", flow2Spin);

    QSpinBox *ovenTempSpin = new QSpinBox(instrGroup);
    ovenTempSpin->setRange(1, 1000);
    ovenTempSpin->setSuffix(" ℃");
    instrForm->addRow("柱温箱温度:", ovenTempSpin);

    QSpinBox *tcdTempSpin = new QSpinBox(instrGroup);
    tcdTempSpin->setRange(0, 500);
    tcdTempSpin->setSuffix(" ℃");
    tcdTempSpin->setToolTip("TS 98\r  设置TCD温度");
    instrForm->addRow("TCD温度:", tcdTempSpin);

    mainLayout->addWidget(instrGroup);

    // ================= TCD 参数 =================
    QGroupBox *tcdGroup = new QGroupBox("TCD 参数", &dlg);
    QFormLayout *tcdForm = new QFormLayout(tcdGroup);

    QSpinBox *collectPointsSpin = new QSpinBox(tcdGroup);
    collectPointsSpin->setRange(100, 100000);
    QPushButton *setCollectBtn = new QPushButton("设置", tcdGroup);
    QHBoxLayout *collectRow = new QHBoxLayout;
    collectRow->addWidget(collectPointsSpin);
    collectRow->addWidget(setCollectBtn);
    tcdForm->addRow("采集点数:", collectRow);

    QSpinBox *averageSpin = new QSpinBox(tcdGroup);
    averageSpin->setRange(1, 1000);
    QPushButton *setAverageBtn = new QPushButton("设置", tcdGroup);
    QHBoxLayout *averageRow = new QHBoxLayout;
    averageRow->addWidget(averageSpin);
    averageRow->addWidget(setAverageBtn);
    tcdForm->addRow("均点设置:", averageRow);

    // 最小精度
    QComboBox *precisionCombo = new QComboBox(tcdGroup);
    precisionCombo->addItem("0.001", 1);
    precisionCombo->addItem("0.01", 10);
    precisionCombo->addItem("0.05", 50);
    precisionCombo->addItem("0.1", 100);
    precisionCombo->addItem("1", 1000);
    precisionCombo->setToolTip("PF 0.05\r   设置最小精度[]");
    tcdForm->addRow("最小精度:", precisionCombo);

    // A 电平
    QDoubleSpinBox *levelASpin = new QDoubleSpinBox(tcdGroup);
    levelASpin->setRange(-1.2, 1.2);
    levelASpin->setDecimals(1);
    levelASpin->setSingleStep(0.1);
    levelASpin->setSuffix(" V");
    levelASpin->setToolTip("ZA 0.1\r");
    tcdForm->addRow("A电平:", levelASpin);

    // B 电平
    QDoubleSpinBox *levelBSpin = new QDoubleSpinBox(tcdGroup);
    levelBSpin->setRange(-1.2, 1.2);
    levelBSpin->setDecimals(1);
    levelBSpin->setSingleStep(0.1);
    levelBSpin->setSuffix(" V");
    levelBSpin->setToolTip("ZB 0.1\r");
    tcdForm->addRow("B电平:", levelBSpin);

    // AB 电平
    QDoubleSpinBox *levelABSpin = new QDoubleSpinBox(tcdGroup);
    levelABSpin->setRange(-1.2, 1.2);
    levelABSpin->setDecimals(1);
    levelABSpin->setSingleStep(0.1);
    levelABSpin->setSuffix(" V");
    levelABSpin->setToolTip("ZR 0.1\r");
    tcdForm->addRow("AB电平:", levelABSpin);

    mainLayout->addWidget(tcdGroup);

    // ================= 底部按钮 =================
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *applyBtn = new QPushButton("应用", &dlg);
    QPushButton *closeBtn = new QPushButton("关闭", &dlg);
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    // ================= 从 QSettings 读取参数 =================
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

    // 电平从 QSettings 中读回 ×10 后的整数值，显示时除以 10
    levelASpin->setValue(settings.value("global/levelA", 0).toInt() / 10.0);
    levelBSpin->setValue(settings.value("global/levelB", 0).toInt() / 10.0);
    levelABSpin->setValue(settings.value("global/levelAB", 0).toInt() / 10.0);

    // ================= 内部“设置”按钮 =================
    connect(setCollectBtn, &QPushButton::clicked, this, [this, collectPointsSpin]() {
        if (m_monitorTab) m_monitorTab->setCollectPoints(collectPointsSpin->value());
        QSettings s("MyCompany", "MicroGC");
        s.setValue("global/collectPoints", collectPointsSpin->value());
        s.sync();
        m_log->appendLog("设置", QString("采集点数设置为 %1").arg(collectPointsSpin->value()));
    });
    connect(setAverageBtn, &QPushButton::clicked, this, [this, averageSpin]() {
        if (m_monitorTab) m_monitorTab->setAveragePoints(averageSpin->value());
        QSettings s("MyCompany", "MicroGC");
        s.setValue("global/averagePoints", averageSpin->value());
        s.sync();
        m_log->appendLog("设置", QString("均点设置设置为 %1").arg(averageSpin->value()));
    });

    // ================= 应用按钮 =================
    connect(applyBtn, &QPushButton::clicked, &dlg, [&]() {
        // 1. 保存到 QSettings
        settings.setValue("global/flow1Voltage", flow1Spin->value());
        settings.setValue("global/flow2Voltage", flow2Spin->value());
        settings.setValue("global/ovenTemp", ovenTempSpin->value());
        settings.setValue("global/tcdTemp", tcdTempSpin->value());
        settings.setValue("global/collectPoints", collectPointsSpin->value());
        settings.setValue("global/averagePoints", averageSpin->value());
        settings.setValue("global/precision", precisionCombo->currentData().toInt());
        // 电平保存为 ×10 后的整数值
        settings.setValue("global/levelA", qRound(levelASpin->value() * 10.0));
        settings.setValue("global/levelB", qRound(levelBSpin->value() * 10.0));
        settings.setValue("global/levelAB", qRound(levelABSpin->value() * 10.0));
        settings.sync();

        // 2. 更新 MonitorTab 界面控件值
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

        // 3. 延迟发送流量电压和柱温箱
        int flow1Val = flow1Spin->value();
        int flow2Val = flow2Spin->value();
        int ovenTemp = ovenTempSpin->value();
        QTimer::singleShot(1500, this, [this, flow1Val, flow2Val, ovenTemp]() {
            if (!m_comm) return;

            // 批量写流量电压 0x0402~0x0403
            QVector<quint16> flowVals = {
                static_cast<quint16>(flow1Val),
                static_cast<quint16>(flow2Val)
            };
            m_comm->writeMultipleRegisters(0x0402, flowVals, "全局设置: 流量电压1+电压2");

            // 柱温箱温度+使能
            m_comm->requestRegisterRead(0x0400, [this, ovenTemp](quint16) {
                QVector<quint16> ovenVals = {
                    static_cast<quint16>(ovenTemp),
                    1
                };
                m_comm->writeMultipleRegisters(0x03FF, ovenVals, "全局设置: 柱温箱温度+使能");
            });
        });

        m_log->appendLog("设置", "全局参数已应用并保存");
        m_log->appendFileOnly("设置", "全局参数已应用并保存");
    });

    dlg.exec();
}

void MainWindow::openAutoProcessSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle("自动流程设置");
    dlg.setMinimumWidth(450);

    QFormLayout form(&dlg);

    QSettings settings("MyCompany", "MicroGC");
    int columnOvenTemp = settings.value("auto/columnOvenTemp", 60).toInt();
    int tcdTemp = settings.value("auto/tcdTemp", 120).toInt();
    int carrierFlow1 = settings.value("auto/carrierFlow1Voltage", 5).toInt();
    int carrierFlow2 = settings.value("auto/carrierFlow2Voltage", 5).toInt();
    double tempTolerance = settings.value("auto/tempTolerance", 0.5).toDouble();
    double startRecordMin = settings.value("auto/startRecordMin", 0.01).toDouble();
    double valveOpenMin = settings.value("auto/valveOpenMin", 0.1).toDouble();
    double valveCloseMin = settings.value("auto/valveCloseMin", 0.8).toDouble();
    double stopRecordMin = settings.value("auto/stopRecordMin", 2.0).toDouble();
    double coolDownTemp = settings.value("auto/coolDownTemp", 50.0).toDouble();

    QSpinBox *ovenSpin = new QSpinBox(&dlg);
    ovenSpin->setRange(1, 1000);
    ovenSpin->setValue(columnOvenTemp);
    ovenSpin->setSuffix(" ℃");

    QSpinBox *tcdSpin = new QSpinBox(&dlg);
    tcdSpin->setRange(0, 500);
    tcdSpin->setValue(tcdTemp);
    tcdSpin->setSuffix(" ℃");

    QSpinBox *flow1Spin = new QSpinBox(&dlg);
    flow1Spin->setRange(1, 1000);
    flow1Spin->setValue(carrierFlow1);
    flow1Spin->setSuffix(" 电压");

    QSpinBox *flow2Spin = new QSpinBox(&dlg);
    flow2Spin->setRange(1, 1000);
    flow2Spin->setValue(carrierFlow2);
    flow2Spin->setSuffix(" 电压");

    QDoubleSpinBox *toleranceSpin = new QDoubleSpinBox(&dlg);
    toleranceSpin->setRange(0.1, 5.0);
    toleranceSpin->setDecimals(1);
    toleranceSpin->setValue(tempTolerance);
    toleranceSpin->setSuffix(" ℃");

    QDoubleSpinBox *startRecSpin = new QDoubleSpinBox(&dlg);
    startRecSpin->setRange(0.0, 10.0);
    startRecSpin->setDecimals(2);
    startRecSpin->setValue(startRecordMin);
    startRecSpin->setSuffix(" min");

    QDoubleSpinBox *valveOpenSpin = new QDoubleSpinBox(&dlg);
    valveOpenSpin->setRange(0.0, 10.0);
    valveOpenSpin->setDecimals(2);
    valveOpenSpin->setValue(valveOpenMin);
    valveOpenSpin->setSuffix(" min");

    QDoubleSpinBox *valveCloseSpin = new QDoubleSpinBox(&dlg);
    valveCloseSpin->setRange(0.0, 10.0);
    valveCloseSpin->setDecimals(2);
    valveCloseSpin->setValue(valveCloseMin);
    valveCloseSpin->setSuffix(" min");

    QDoubleSpinBox *stopRecSpin = new QDoubleSpinBox(&dlg);
    stopRecSpin->setRange(0.0, 60.0);
    stopRecSpin->setDecimals(2);
    stopRecSpin->setValue(stopRecordMin);
    stopRecSpin->setSuffix(" min");

    QDoubleSpinBox *coolSpin = new QDoubleSpinBox(&dlg);
    coolSpin->setRange(0.0, 100.0);
    coolSpin->setDecimals(1);
    coolSpin->setValue(coolDownTemp);
    coolSpin->setSuffix(" ℃");

    form.addRow("柱温箱目标温度:", ovenSpin);
    form.addRow("TCD目标温度:", tcdSpin);
    form.addRow("流量器A电压:", flow1Spin);
    form.addRow("流量器B电压:", flow2Spin);
    form.addRow("温度稳定容差:", toleranceSpin);
    form.addRow("开始记录时间:", startRecSpin);
    form.addRow("六通阀开启时间:", valveOpenSpin);
    form.addRow("六通阀关闭时间:", valveCloseSpin);
    form.addRow("停止记录时间:", stopRecSpin);
    form.addRow("冷却安全温度:", coolSpin);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        settings.setValue("auto/columnOvenTemp", ovenSpin->value());
        settings.setValue("auto/tcdTemp", tcdSpin->value());
        settings.setValue("auto/carrierFlow1Voltage", flow1Spin->value());
        settings.setValue("auto/carrierFlow2Voltage", flow2Spin->value());
        settings.setValue("auto/tempTolerance", toleranceSpin->value());
        settings.setValue("auto/startRecordMin", startRecSpin->value());
        settings.setValue("auto/valveOpenMin", valveOpenSpin->value());
        settings.setValue("auto/valveCloseMin", valveCloseSpin->value());
        settings.setValue("auto/stopRecordMin", stopRecSpin->value());
        settings.setValue("auto/coolDownTemp", coolSpin->value());

        AutoProcessSettings aps;
        aps.columnOvenTemp = ovenSpin->value();
        aps.tcdTemp = tcdSpin->value();
        aps.carrierFlow1Voltage = flow1Spin->value();
        aps.carrierFlow2Voltage = flow2Spin->value();
        aps.tempTolerance = toleranceSpin->value();
        aps.startRecordMin = startRecSpin->value();
        aps.valveOpenMin = valveOpenSpin->value();
        aps.valveCloseMin = valveCloseSpin->value();
        aps.stopRecordMin = stopRecSpin->value();
        aps.coolDownTemp = coolSpin->value();
        m_autoProcess->setSettings(aps);

        m_log->appendLog("自动流程", "设置已保存");
        m_log->appendFileOnly("自动流程", "设置已保存");
    }
}

void MainWindow::startStopAutoProcess()
{
    if (!m_autoProcess) return;
    if (m_autoProcess->currentStateName() == "空闲") {
        m_autoProcess->start();
    } else {
        m_autoProcess->stop();
    }
}

void MainWindow::resetSystem()
{
    if (m_autoProcess) {
        m_autoProcess->reset();
    }
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

    // 读缓存的故障诊断寄存器（慢速轮询每秒更新一次）
    quint16 diag = m_comm->diagRegister();
    bool tc4Fail = (diag & (1 << 0)) != 0;   // BIT0: TC4通信故障
    bool tcdFail = (diag & (1 << 1)) != 0;   // BIT1: TCD通信故障
    bool powered = !(tc4Fail || tcdFail);

    m_tcdPowered = powered;
    m_comm->setTcdPowered(powered);
    updateTcdPowerIcon();
    if (m_monitorTab) m_monitorTab->setTcdPowered(powered);
}

void MainWindow::updateTcdPowerIcon()
{
    if (m_tcdPowerAction) {
        if (m_tcdPowered) {
            m_tcdPowerAction->setIcon(m_iconTcdOn);
            m_tcdPowerAction->setText("TCD电源：已通电");
        } else {
            m_tcdPowerAction->setIcon(m_iconTcdOff);
            m_tcdPowerAction->setText("TCD电源：未连接");
        }
    }
}