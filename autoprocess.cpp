#include "autoprocess.h"
#include "communication.h"

#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

//==========================================================
// 辅助函数：返回 " | TCD=xx.xxs" 的字符串
//   若未连接 TCD 时间提供者，返回空串，日志自然不带后缀
//==========================================================
static QString tcdTag(const std::function<double()> &provider)
{
    if (!provider) return QString();
    double tcd = provider();
    return QString(" | TCD=%1s").arg(tcd, 0, 'f', 2);
}

//==========================================================
// 构造
//==========================================================
AutoProcessManager::AutoProcessManager(Communication *comm, QObject *parent)
    : QObject(parent)
    , m_comm(comm)
    , m_tickTimer(new QTimer(this))
{
    m_tickTimer->setInterval(50);   // 50ms 检查一次
    connect(m_tickTimer, &QTimer::timeout, this, &AutoProcessManager::onTick);
    loadFromSettings();

    // 加载自动测量默认配置
    QSettings settings("MyCompany", "MicroGC");
    m_totalCycles    = settings.value("auto/cycleCount", 1).toInt();
    m_autoSavePath   = settings.value("auto/savePath", "./auto_data").toString();
    m_autoFilePrefix = settings.value("auto/filePrefix", "auto_measure").toString();
}

//==========================================================
// TCD 时间提供者
//==========================================================
void AutoProcessManager::setTcdTimeProvider(std::function<double()> provider)
{
    m_tcdTimeProvider = provider;
}

//==========================================================
// 事件组读写
//==========================================================
void AutoProcessManager::setGroups(const QList<AutoEventGroup> &groups)
{
    m_groups = groups;
    saveToSettings();
}

QString AutoProcessManager::currentStateName() const
{
    return m_running ? "运行中" : "空闲";
}

//==========================================================
// 启动 / 停止
//==========================================================
void AutoProcessManager::start()
{
    if (m_running) return;

    if (!m_comm || !m_comm->isConnected()) {
        emit logMessage("自动流程", "通信未连接，无法开始");
        return;
    }
    if (m_groups.isEmpty()) {
        emit logMessage("自动流程", "至少需要一组事件");
        return;
    }

    bool hasCollect = false;
    for (const auto &g : m_groups) {
        if (g.type == AutoEventType::Collect) { hasCollect = true; break; }
    }
    if (!hasCollect) {
        emit logMessage("自动流程", "必须包含数据采集事件组");
        return;
    }

    // 启动前重新读取全局配置
    QSettings settings("MyCompany", "MicroGC");
    m_totalCycles    = settings.value("auto/cycleCount", 1).toInt();
    m_autoSavePath   = settings.value("auto/savePath", "./auto_data").toString();
    m_autoFilePrefix = settings.value("auto/filePrefix", "auto_measure").toString();

    m_currentCycle = 1;

    emit logMessage("自动流程",
                    QString("用户点击开始测量%1").arg(tcdTag(m_tcdTimeProvider)));

    startSingleCycle();
}

void AutoProcessManager::startSingleCycle()
{
    buildExecutionQueue();
    if (m_queue.isEmpty()) {
        emit logMessage("自动流程", "执行队列为空");
        return;
    }

    m_currentIndex = 0;
    m_running      = true;
    m_startMs      = QDateTime::currentMSecsSinceEpoch();
    changeState();

    emit logMessage("自动流程",
                    QString("第 %1/%2 次测量启动，共 %3 个事件%4")
                        .arg(m_currentCycle)
                        .arg(m_totalCycles)
                        .arg(m_queue.size())
                        .arg(tcdTag(m_tcdTimeProvider)));

    m_tickTimer->start();
}

void AutoProcessManager::stop()
{
    if (!m_running) return;

    m_tickTimer->stop();
    m_running      = false;
    m_currentIndex = 0;
    m_queue.clear();
    m_currentCycle = 0;
    changeState();

    emit logMessage("自动流程",
                    QString("用户停止，后续事件不再执行%1")
                        .arg(tcdTag(m_tcdTimeProvider)));
    emit finished();
}

//==========================================================
// 定时检查（含循环跳转）
//==========================================================
void AutoProcessManager::onTick()
{
    if (!m_running) return;

    qint64 elapsedMs  = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    double elapsedMin = elapsedMs / 60000.0;

    while (m_currentIndex < m_queue.size() &&
           elapsedMin >= m_queue[m_currentIndex].timeMin)
    {
        executeEvent(m_queue[m_currentIndex]);
        m_currentIndex++;
    }

    if (m_currentIndex >= m_queue.size()) {
        m_tickTimer->stop();

        QString tag = tcdTag(m_tcdTimeProvider);
        emit logMessage("自动流程",
                        QString("第 %1 次测量事件执行完毕%2")
                            .arg(m_currentCycle)
                            .arg(tag));

        if (m_currentCycle < m_totalCycles) {
            m_currentCycle++;
            QTimer::singleShot(1000, this, [this]() {
                startSingleCycle();
            });
        } else {
            m_running = false;
            changeState();
            emit logMessage("自动流程",
                            QString("全部 %1 次测量完成%2")
                                .arg(m_totalCycles)
                                .arg(tag));
            emit finished();
        }
    }
}

//==========================================================
// 构建执行队列（按时间排序）
//==========================================================
void AutoProcessManager::buildExecutionQueue()
{
    m_queue.clear();

    for (const auto &g : m_groups) {
        EventItem on;
        on.type     = g.type;
        on.onAction = true;
        on.timeMin  = g.onTimeMin;
        m_queue.append(on);

        EventItem off;
        off.type     = g.type;
        off.onAction = false;
        off.timeMin  = g.offTimeMin;
        m_queue.append(off);
    }

    std::sort(m_queue.begin(), m_queue.end(),
              [](const EventItem &a, const EventItem &b) {
                  return a.timeMin < b.timeMin;
              });
}

//==========================================================
// 执行单个事件
//==========================================================
void AutoProcessManager::executeEvent(const EventItem &e)
{
    QString tag = tcdTag(m_tcdTimeProvider);

    switch (e.type) {

    case AutoEventType::Collect:
        if (e.onAction) {
            emit startDataSaveTriggered(m_autoSavePath, m_autoFilePrefix, m_currentCycle);
            emit logMessage("自动流程",
                            QString("数据采集开始 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        } else {
            emit stopDataSaveTriggered();
            emit logMessage("自动流程",
                            QString("数据采集停止 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        }
        break;

    case AutoEventType::Valve:
        if (e.onAction) {
            // 先开电磁阀，再开六通阀
            m_comm->setValveBit(0, true);
            m_comm->setSixWayValve1(true);
            emit deviceStateChanged("电磁阀", true);
            emit deviceStateChanged("六通阀", true);
            emit logMessage("自动流程",
                            QString("六通阀开启：先电磁阀 后六通阀 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        } else {
            // 先关电磁阀，再关六通阀
            m_comm->setValveBit(0, false);
            m_comm->setSixWayValve1(false);
            emit deviceStateChanged("电磁阀", false);
            emit deviceStateChanged("六通阀", false);
            emit logMessage("自动流程",
                            QString("六通阀关闭：先电磁阀 后六通阀 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        }
        break;

    case AutoEventType::Polarity:
        if (e.onAction) {
            emit polarityOnTriggered();
            emit logMessage("自动流程",
                            QString("极性反转开启 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        } else {
            emit polarityOffTriggered();
            emit logMessage("自动流程",
                            QString("极性反转关闭 (t=%1 min%2)")
                                .arg(e.timeMin, 0, 'f', 3)
                                .arg(tag));
        }
        break;
    }
}

void AutoProcessManager::changeState()
{
    emit stateChanged(currentStateName());
}

//==========================================================
// QSettings 保存/加载
//==========================================================
void AutoProcessManager::saveToSettings() const
{
    QSettings settings("MyCompany", "MicroGC");
    settings.beginWriteArray("auto/groups");
    for (int i = 0; i < m_groups.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("type",    static_cast<int>(m_groups[i].type));
        settings.setValue("onTime",  m_groups[i].onTimeMin);
        settings.setValue("offTime", m_groups[i].offTimeMin);
    }
    settings.endArray();
    settings.sync();
}

void AutoProcessManager::loadFromSettings()
{
    QSettings settings("MyCompany", "MicroGC");
    int size = settings.beginReadArray("auto/groups");
    m_groups.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        AutoEventGroup g;
        g.type       = static_cast<AutoEventType>(settings.value("type", 0).toInt());
        g.onTimeMin  = settings.value("onTime",  0.0).toDouble();
        g.offTimeMin = settings.value("offTime", 1.0).toDouble();
        m_groups.append(g);
    }
    settings.endArray();
}

//==========================================================
// JSON 文件导入导出
//==========================================================
bool AutoProcessManager::exportToFile(const QString &fileName) const
{
    QJsonObject root;
    QJsonArray arr;

    for (const auto &g : m_groups) {
        QJsonObject obj;
        obj["type"]       = static_cast<int>(g.type);
        obj["onTimeMin"]  = g.onTimeMin;
        obj["offTimeMin"] = g.offTimeMin;
        arr.append(obj);
    }
    root["version"] = 1;
    root["groups"]  = arr;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool AutoProcessManager::importFromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    QJsonObject root = doc.object();
    QJsonArray  arr  = root["groups"].toArray();

    QList<AutoEventGroup> newGroups;
    for (const auto &v : arr) {
        QJsonObject obj = v.toObject();
        AutoEventGroup g;
        g.type       = static_cast<AutoEventType>(obj["type"].toInt());
        g.onTimeMin  = obj["onTimeMin"].toDouble();
        g.offTimeMin = obj["offTimeMin"].toDouble();
        newGroups.append(g);
    }

    m_groups = newGroups;
    saveToSettings();
    return true;
}