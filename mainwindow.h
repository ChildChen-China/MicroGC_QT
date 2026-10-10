#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusTcpClient>
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
    void startStopAutoProcess();
    void resetSystem();
    void toggleLogVisible(bool visible);
    void onCommunicationConnected();
    void onCommunicationDisconnected();
    void checkTcdPowerStatus();
    void updateTcdPowerIcon();

    void onOvenToggle();                // TC4 加热开关
    void updateOvenIcon();

    void onFaultBitsChanged(quint16 diag);   // 0x000A 故障位变化

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
    QAction *m_startStopAction;
    QAction *m_connectAction = nullptr;
    QIcon m_iconConnectDefault;

    // TCD 电源状态指示
    QAction *m_tcdPowerAction = nullptr;
    QTimer *m_tcdCheckTimer = nullptr;
    bool m_tcdPowered = false;
    QIcon m_iconTcdOn;
    QIcon m_iconTcdOff;

    // TC4 加热开关按钮
    QAction *m_ovenAction = nullptr;
    bool m_ovenEnabled = false;
    QIcon m_iconOvenOff;
    QIcon m_iconOvenOn;

    // 故障位缓存（用于去重）
    quint16 m_lastDiag = 0xFFFF;
};

#endif // MAINWINDOW_H