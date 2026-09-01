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
};

#endif // MAINWINDOW_H