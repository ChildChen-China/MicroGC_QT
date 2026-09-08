#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusTcpClient>
#include <QPainter>
#include <QPixmap>
#include "autoprocess.h"

class QTabWidget;
class LogWidget;
class Communication;
class MonitorTab;
class ControlTab;
class OtherTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openConnectionDialog();
    void openDataProcessing();
    void openSettings();
    void openAutoProcessSettings();
    void startStopAutoProcess();       // 合并的开始/停止槽
    void resetSystem();
    void toggleLogVisible(bool visible);
    void onCommunicationConnected();
    void onCommunicationDisconnected();

private:
    void createActions();
    void createTabs();

    QTabWidget *m_tabs;
    LogWidget *m_log;
    Communication *m_comm;
    MonitorTab *m_monitorTab;
    ControlTab *m_controlTab;
    OtherTab *m_otherTab;

    AutoProcessManager *m_autoProcess;
    QAction *m_startStopAction;        // 开始/停止切换按钮动作

    int m_flow1Setpoint = 200;
    int m_flow2Setpoint = 100;

    // TCD 电源状态指示
    QAction *m_tcdPowerAction = nullptr;
    QTimer *m_tcdCheckTimer = nullptr;
    bool m_tcdPowered = false;
    bool m_tcdReplyReceived = false;
    bool m_checkActive = false;
    QIcon m_iconTcdOn;
    QIcon m_iconTcdOff;

    QAction *m_connectAction = nullptr;   // 连接动作指针
    QIcon m_iconConnectDefault;           // 默认连接图标

    void updateTcdPowerIcon();
    void checkTcdPowerStatus();
};

#endif // MAINWINDOW_H