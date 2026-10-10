#ifndef AUTOPROCESS_H
#define AUTOPROCESS_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QString>

class Communication;

//==========================================================
// 事件组类型
//==========================================================
enum class AutoEventType {
    Collect,    // 数据采集
    Valve,      // 六通阀（先电磁阀后六通阀，绑定动作）
    Polarity    // 极性反转（仅影响显示和保存取负，不动硬件）
};

//==========================================================
// 一个事件组：包含"开"和"关"两个时刻（单位：分钟）
//==========================================================
struct AutoEventGroup {
    AutoEventType type = AutoEventType::Collect;
    double onTimeMin = 0.0;
    double offTimeMin = 1.0;
};

//==========================================================
// 自动流程管理器
// 职责：
//   1. 保存用户配置的事件组
//   2. 启动后从 t=0 开始计时
//   3. 到达时间点依次触发事件
//   4. 全部执行完自动停止，或用户手动停止后不再执行
//==========================================================
class AutoProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit AutoProcessManager(Communication *comm, QObject *parent = nullptr);

    QList<AutoEventGroup> groups() const { return m_groups; }
    void setGroups(const QList<AutoEventGroup> &groups);

    // QSettings 自动保存/加载
    void saveToSettings() const;
    void loadFromSettings();

    // JSON 文件导入导出
    bool exportToFile(const QString &fileName) const;
    bool importFromFile(const QString &fileName);

    QString currentStateName() const;
    bool isRunning() const { return m_running; }

signals:
    void stateChanged(const QString &stateName);
    void logMessage(const QString &type, const QString &event);
    void deviceStateChanged(const QString &device, bool state);
    // 数据采集控制
    void startDataSaveTriggered();
    void stopDataSaveTriggered();

    // 极性反转控制（通知 MonitorTab 记录反转时间段）
    void polarityOnTriggered();
    void polarityOffTriggered();

    void finished();

public slots:
    void start();
    void stop();

private slots:
    void onTick();

private:
    struct EventItem {
        AutoEventType type;
        bool onAction;      // true=开, false=关
        double timeMin;
    };

    void buildExecutionQueue();
    void executeEvent(const EventItem &e);
    void changeState();

    Communication *m_comm;
    QList<AutoEventGroup> m_groups;
    QList<EventItem> m_queue;
    int m_currentIndex = 0;
    bool m_running = false;

    QTimer *m_tickTimer;
    qint64 m_startMs = 0;
};

#endif // AUTOPROCESS_H