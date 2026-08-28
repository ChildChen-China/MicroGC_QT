#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModbusTcpClient>

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
    void startAutoProcess();
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
};

#endif // MAINWINDOW_H