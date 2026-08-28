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
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabs(nullptr)
    , m_log(nullptr)
    , m_comm(nullptr)
    , m_monitorTab(nullptr)
    , m_controlTab(nullptr)
    , m_otherTab(nullptr)
{
    setWindowTitle("MicroGC");
    resize(1500, 900);

    m_comm = new Communication(this);
    m_log = new LogWidget(this);

    // 连接通信信号
    connect(m_comm, &Communication::logPacket, this, [this](const QString &direction, const QString &dataHex) {
        m_log->appendLog("通信", QString("[%1] %2").arg(direction, dataHex));
    });
    connect(m_comm, &Communication::statusMessage, this, [this](const QString &msg) {
        m_log->appendLog("通信", msg);
    });
    connect(m_comm, &Communication::connected, this, &MainWindow::onCommunicationConnected);
    connect(m_comm, &Communication::disconnected, this, &MainWindow::onCommunicationDisconnected);

    createActions();
    createTabs();

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_log, 0);
    setCentralWidget(central);
}

void MainWindow::createActions()
{
    QToolBar *toolbar = addToolBar("主工具栏");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QAction *connectAct = toolbar->addAction(style()->standardIcon(QStyle::SP_DriveNetIcon), "连接");
    QAction *dataAct = toolbar->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), "数据处理");

    QToolButton *settingsBtn = new QToolButton(this);
    settingsBtn->setText("设置");
    settingsBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogInfoView));
    settingsBtn->setPopupMode(QToolButton::InstantPopup);
    settingsBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    QMenu *settingsMenu = new QMenu(settingsBtn);
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
    settingsBtn->setMenu(settingsMenu);
    toolbar->addWidget(settingsBtn);

    QAction *startAct = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "开始");
    QAction *resetAct = toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), "复位");

    connect(connectAct, &QAction::triggered, this, &MainWindow::openConnectionDialog);
    connect(dataAct, &QAction::triggered, this, &MainWindow::openDataProcessing);
    connect(globalSettingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    connect(startAct, &QAction::triggered, this, &MainWindow::startAutoProcess);
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

void MainWindow::createTabs()
{
    m_tabs = new QTabWidget(this);

    m_controlTab = new ControlTab;
    m_monitorTab = new MonitorTab;
    m_otherTab = new OtherTab;

    m_tabs->addTab(m_controlTab, "控制");
    m_tabs->addTab(m_monitorTab, "监视");
    m_tabs->addTab(m_otherTab, "其他");

    // 将通信对象传递给各页面
    m_controlTab->setCommunication(m_comm);
    m_monitorTab->setCommunication(m_comm);
    m_otherTab->setCommunication(m_comm);

    // 连接控制页信号
    connect(m_controlTab, &ControlTab::commandRequested, this, [this](const QString &device, bool state) {
        // 根据设备名发送对应命令
        if (device.startsWith("NV")) {
            // 电磁阀整体或单个？当前协议只有整体寄存器，暂时忽略
            m_log->appendLog("控制", QString("阀门 %1 状态:%2").arg(device).arg(state ? "开启" : "关闭"));
        }
    });
    connect(m_controlTab, &ControlTab::logMessage, this, [this](const QString &type, const QString &event) {
        m_log->appendLog(type, event);
    });
    connect(m_controlTab, &ControlTab::sixWayValveToggled, this, [this]() {
        // 六通阀切换，触发保存（监视页已有）
        if (m_monitorTab) m_monitorTab->startAutoSave();
    });

    // 其他页面日志
    connect(m_monitorTab, &MonitorTab::logMessage, this, [this](const QString &type, const QString &event) {
        m_log->appendLog(type, event);
    });
    connect(m_otherTab, &OtherTab::logMessage, this, [this](const QString &type, const QString &event) {
        m_log->appendLog(type, event);
    });
}

void MainWindow::onCommunicationConnected()
{
    m_comm->startPolling();
    m_log->appendLog("通信", "已连接并开始轮询");
}

void MainWindow::onCommunicationDisconnected()
{
    m_comm->stopPolling();
    m_log->appendLog("通信", "连接断开");
}

void MainWindow::openConnectionDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Modbus-TCP 连接设置");
    dlg.setMinimumWidth(360);

    QFormLayout form(&dlg);
    QLineEdit *ipEdit = new QLineEdit("127.0.0.1", &dlg);
    QSpinBox *portSpin = new QSpinBox(&dlg);
    portSpin->setRange(1, 65535);
    portSpin->setValue(502);

    form.addRow("PLC IP 地址:", ipEdit);
    form.addRow("PLC 端口:", portSpin);

    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        m_comm->connectToDevice(ipEdit->text(), portSpin->value());
        m_log->appendLog("连接", QString("尝试连接 %1:%2").arg(ipEdit->text()).arg(portSpin->value()));
    }
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

    // ================= 第一块：流量控制器 =================
    QGroupBox *flowGroup = new QGroupBox("流量控制器", &dlg);
    QFormLayout *flowForm = new QFormLayout(flowGroup);

    QSpinBox *flow1Spin = new QSpinBox(flowGroup);
    flow1Spin->setRange(0, 10000);
    flow1Spin->setSuffix(" mL/min");
    flow1Spin->setValue(200);

    QSpinBox *flow2Spin = new QSpinBox(flowGroup);
    flow2Spin->setRange(0, 10000);
    flow2Spin->setSuffix(" mL/min");
    flow2Spin->setValue(100);

    flowForm->addRow("流量1:", flow1Spin);
    flowForm->addRow("流量2:", flow2Spin);

    // 采集点数 + 设置按钮
    QSpinBox *collectPointsSpin = new QSpinBox(flowGroup);
    collectPointsSpin->setRange(100, 100000);
    collectPointsSpin->setValue(1000);
    QPushButton *setCollectBtn = new QPushButton("设置", flowGroup);
    QHBoxLayout *collectRow = new QHBoxLayout;
    collectRow->addWidget(collectPointsSpin);
    collectRow->addWidget(setCollectBtn);
    flowForm->addRow("采集点数:", collectRow);

    // 均点设置 + 设置按钮
    QSpinBox *averageSpin = new QSpinBox(flowGroup);
    averageSpin->setRange(1, 1000);
    averageSpin->setValue(5);
    QPushButton *setAverageBtn = new QPushButton("设置", flowGroup);
    QHBoxLayout *averageRow = new QHBoxLayout;
    averageRow->addWidget(averageSpin);
    averageRow->addWidget(setAverageBtn);
    flowForm->addRow("均点设置:", averageRow);

    mainLayout->addWidget(flowGroup);

    // ================= 第二块：TCD 参数 =================
    QGroupBox *tcdGroup = new QGroupBox("TCD 参数", &dlg);
    QFormLayout *tcdForm = new QFormLayout(tcdGroup);

    QSpinBox *tempSpin = new QSpinBox(tcdGroup);
    tempSpin->setRange(-100, 500);
    tempSpin->setSuffix(" ℃");
    tempSpin->setValue(25);

    QSpinBox *powerASpin = new QSpinBox(tcdGroup);
    powerASpin->setRange(0, 100);
    powerASpin->setSuffix(" %");
    powerASpin->setValue(50);

    QSpinBox *powerBSpin = new QSpinBox(tcdGroup);
    powerBSpin->setRange(0, 100);
    powerBSpin->setSuffix(" %");
    powerBSpin->setValue(50);

    QComboBox *precisionCombo = new QComboBox(tcdGroup);
    precisionCombo->addItem("0.01", 1);
    precisionCombo->addItem("0.1", 10);
    precisionCombo->addItem("1", 100);
    precisionCombo->setCurrentIndex(0);

    QSpinBox *levelASpin = new QSpinBox(tcdGroup);
    levelASpin->setRange(-1000, 1000);
    levelASpin->setSuffix(" mV");
    levelASpin->setValue(0);

    QSpinBox *levelBSpin = new QSpinBox(tcdGroup);
    levelBSpin->setRange(-1000, 1000);
    levelBSpin->setSuffix(" mV");
    levelBSpin->setValue(0);

    QSpinBox *levelABSpin = new QSpinBox(tcdGroup);
    levelABSpin->setRange(-1000, 1000);
    levelABSpin->setSuffix(" mV");
    levelABSpin->setValue(0);

    tcdForm->addRow("设置温度:", tempSpin);
    tcdForm->addRow("灯丝A功率:", powerASpin);
    tcdForm->addRow("灯丝B功率:", powerBSpin);
    tcdForm->addRow("最小精度:", precisionCombo);
    tcdForm->addRow("A电平(mV):", levelASpin);
    tcdForm->addRow("B电平(mV):", levelBSpin);
    tcdForm->addRow("AB电平(mV):", levelABSpin);

    mainLayout->addWidget(tcdGroup);

    // ================= 底部按钮 =================
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *saveBtn = new QPushButton("保存", &dlg);
    QPushButton *openBtn = new QPushButton("打开", &dlg);
    QPushButton *applyBtn = new QPushButton("应用", &dlg);
    QPushButton *closeBtn = new QPushButton("关闭", &dlg);
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(openBtn);
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    // 采集点数/均点即时设置
    connect(setCollectBtn, &QPushButton::clicked, this, [this, collectPointsSpin]() {
        if (m_monitorTab) m_monitorTab->setCollectPoints(collectPointsSpin->value());
        m_log->appendLog("设置", QString("采集点数设置为 %1").arg(collectPointsSpin->value()));
    });
    connect(setAverageBtn, &QPushButton::clicked, this, [this, averageSpin]() {
        if (m_monitorTab) m_monitorTab->setAveragePoints(averageSpin->value());
        m_log->appendLog("设置", QString("均点设置设置为 %1").arg(averageSpin->value()));
    });

    // 应用按钮：发送所有整数参数
    connect(applyBtn, &QPushButton::clicked, &dlg, [&]() {
        m_comm->setFlow1Setpoint(static_cast<quint16>(flow1Spin->value()));
        m_comm->setFlow2Setpoint(static_cast<quint16>(flow2Spin->value()));
        m_comm->setTcdTemperature(static_cast<quint16>(tempSpin->value()));
        m_comm->setLampPowerA(static_cast<quint16>(powerASpin->value()));
        m_comm->setLampPowerB(static_cast<quint16>(powerBSpin->value()));
        m_comm->setPrecision(static_cast<quint16>(precisionCombo->currentData().toInt()));
        m_comm->setChannelAVoltage(static_cast<quint16>(levelASpin->value()));
        m_comm->setChannelBVoltage(static_cast<quint16>(levelBSpin->value()));
        m_comm->setChannelABVoltage(static_cast<quint16>(levelABSpin->value()));
        m_log->appendLog("设置", "全局参数已应用");
    });

    // 保存全局参数到文件
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        QString fileName = QFileDialog::getSaveFileName(&dlg, "保存全局参数", QString(), "配置文件 (*.ini)");
        if (fileName.isEmpty()) return;

        QSettings settings(fileName, QSettings::IniFormat);
        settings.setValue("flow1", flow1Spin->value());
        settings.setValue("flow2", flow2Spin->value());
        settings.setValue("collectPoints", collectPointsSpin->value());
        settings.setValue("averagePointsFlow", averageSpin->value());
        settings.setValue("temperature", tempSpin->value());
        settings.setValue("powerA", powerASpin->value());
        settings.setValue("powerB", powerBSpin->value());
        settings.setValue("precision", precisionCombo->currentData().toInt());
        settings.setValue("levelA", levelASpin->value());
        settings.setValue("levelB", levelBSpin->value());
        settings.setValue("levelAB", levelABSpin->value());
        settings.sync();

        m_log->appendLog("设置", QString("全局参数已保存至 %1").arg(fileName));
    });

    // 从文件加载全局参数
    connect(openBtn, &QPushButton::clicked, &dlg, [&]() {
        QString fileName = QFileDialog::getOpenFileName(&dlg, "打开全局参数", QString(), "配置文件 (*.ini)");
        if (fileName.isEmpty()) return;

        QSettings settings(fileName, QSettings::IniFormat);
        flow1Spin->setValue(settings.value("flow1", 200).toInt());
        flow2Spin->setValue(settings.value("flow2", 100).toInt());
        collectPointsSpin->setValue(settings.value("collectPoints", 1000).toInt());
        averageSpin->setValue(settings.value("averagePointsFlow", 5).toInt());
        tempSpin->setValue(settings.value("temperature", 25).toInt());
        powerASpin->setValue(settings.value("powerA", 50).toInt());
        powerBSpin->setValue(settings.value("powerB", 50).toInt());

        int precisionValue = settings.value("precision", 1).toInt();
        int idx = precisionCombo->findData(precisionValue);
        if (idx >= 0) precisionCombo->setCurrentIndex(idx);

        levelASpin->setValue(settings.value("levelA", 0).toInt());
        levelBSpin->setValue(settings.value("levelB", 0).toInt());
        levelABSpin->setValue(settings.value("levelAB", 0).toInt());

        m_log->appendLog("设置", QString("全局参数已从 %1 加载").arg(fileName));
    });

    dlg.exec();
}

void MainWindow::startAutoProcess()
{
    m_log->appendLog("自动流程", "自动分析流程开始");
    if (m_monitorTab) m_monitorTab->startAutoSave();
}

void MainWindow::resetSystem()
{
    m_log->appendLog("复位", "系统复位指令已下发");
}

void MainWindow::toggleLogVisible(bool visible)
{
    if (m_log) m_log->setVisible(!visible);
}