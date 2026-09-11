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

    // TCD 电源指示
    QAction *m_tcdPowerAction = nullptr;
    QTimer *m_tcdCheckTimer = nullptr;
    bool m_tcdPowered = false;
    bool m_tcdReplyReceived = false;
    bool m_checkActive = false;
    QIcon m_iconTcdOn;
    QIcon m_iconTcdOff;

    int m_flow1Voltage = 5;
    int m_flow2Voltage = 5;
};

#endif // MAINWINDOW_H