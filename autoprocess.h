#ifndef AUTOPROCESS_H
#define AUTOPROCESS_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QString>
#include <functional>

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
    double onTimeMin  = 0.0;
    double offTimeMin = 1.0;
};

//==========================================================
// 自动流程管理器
// 职责：
//   1. 保存用户配置的事件组
//   2. 启动后从 t=0 开始计时
//   3. 到达时间点依次触发事件
//   4. 全部执行完自动停止，或用户手动停止后不再执行
//   5. 支持多次循环测量，每次独立生成数据文件
//==========================================================
class AutoProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit AutoProcessManager(Communication *comm, QObject *parent = nullptr);

    // TCD 时间提供者（由 MainWindow 连接 MonitorTab 的 currentTcdSeconds）
    void setTcdTimeProvider(std::function<double()> provider);

    // 事件组读写
    QList<AutoEventGroup> groups() const { return m_groups; }
    void setGroups(const QList<AutoEventGroup> &groups);

    // QSettings 自动保存/加载
    void saveToSettings() const;
    void loadFromSettings();

    // JSON 文件导入导出（供自动流程设置对话框调用）
    bool exportToFile(const QString &fileName) const;
    bool importFromFile(const QString &fileName);

    QString currentStateName() const;
    bool isRunning() const { return m_running; }

signals:
    void stateChanged(const QString &stateName);
    void logMessage(const QString &type, const QString &event);
    void deviceStateChanged(const QString &device, bool state);

    // 数据采集控制（自动测量专用：带路径、文件名前缀、循环序号）
    void startDataSaveTriggered(const QString &path,
                                const QString &fileNamePrefix,
                                int cycleIndex);
    void stopDataSaveTriggered();

    // 极性反转控制
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
        bool   onAction;    // true=开, false=关
        double timeMin;
    };

    void buildExecutionQueue();
    void executeEvent(const EventItem &e);
    void changeState();
    void startSingleCycle();

    // 核心对象
    Communication *m_comm = nullptr;
    QTimer        *m_tickTimer = nullptr;

    // 事件组
    QList<AutoEventGroup> m_groups;
    QList<EventItem>      m_queue;
    int    m_currentIndex = 0;
    bool   m_running = false;
    qint64 m_startMs = 0;

    // 多次循环测量配置
    int     m_totalCycles   = 1;
    int     m_currentCycle  = 0;
    QString m_autoSavePath;
    QString m_autoFilePrefix;

    // TCD 时间提供者
    std::function<double()> m_tcdTimeProvider;
};

#endif // AUTOPROCESS_H