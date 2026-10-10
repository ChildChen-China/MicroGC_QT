#include "autoprocess.h"
#include "communication.h"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

AutoProcessManager::AutoProcessManager(Communication *comm, QObject *parent)
    : QObject(parent)
    , m_comm(comm)
    , m_tickTimer(new QTimer(this))
{
    m_tickTimer->setInterval(50);       // 50ms 检查一次，足够精细
    connect(m_tickTimer, &QTimer::timeout, this, &AutoProcessManager::onTick);

    loadFromSettings();
}

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
// 启动
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

    // 必须包含数据采集组
    bool hasCollect = false;
    for (const auto &g : m_groups) {
        if (g.type == AutoEventType::Collect) {
            hasCollect = true;
            break;
        }
    }
    if (!hasCollect) {
        emit logMessage("自动流程", "必须包含数据采集事件组");
        return;
    }

    buildExecutionQueue();
    if (m_queue.isEmpty()) {
        emit logMessage("自动流程", "执行队列为空");
        return;
    }

    m_currentIndex = 0;
    m_running = true;
    m_startMs = QDateTime::currentMSecsSinceEpoch();

    changeState();
    emit logMessage("自动流程", QString("启动，共 %1 个事件").arg(m_queue.size()));
    m_tickTimer->start();
}

//==========================================================
// 手动停止
//==========================================================
void AutoProcessManager::stop()
{
    if (!m_running) return;

    m_tickTimer->stop();
    m_running = false;
    m_currentIndex = 0;
    m_queue.clear();

    changeState();
    emit logMessage("自动流程", "用户停止，后续事件不再执行");
    emit finished();
}

//==========================================================
// 定时检查
//==========================================================
void AutoProcessManager::onTick()
{
    if (!m_running) return;

    qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    double elapsedMin = elapsedMs / 60000.0;

    while (m_currentIndex < m_queue.size() &&
           elapsedMin >= m_queue[m_currentIndex].timeMin)
    {
        executeEvent(m_queue[m_currentIndex]);
        m_currentIndex++;
    }

    if (m_currentIndex >= m_queue.size()) {
        // 全部执行完毕，自动停止
        m_tickTimer->stop();
        m_running = false;
        changeState();
        emit logMessage("自动流程", "全部事件执行完毕");
        emit finished();
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
        on.type = g.type;
        on.onAction = true;
        on.timeMin = g.onTimeMin;
        m_queue.append(on);

        EventItem off;
        off.type = g.type;
        off.onAction = false;
        off.timeMin = g.offTimeMin;
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
    switch (e.type) {

    case AutoEventType::Collect:
        if (e.onAction) {
            emit startDataSaveTriggered();
            emit logMessage("自动流程",
                            QString("数据采集开始 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
        } else {
            emit stopDataSaveTriggered();
            emit logMessage("自动流程",
                            QString("数据采集停止 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
        }
        break;

    case AutoEventType::Valve:
        if (e.onAction) {
            // 先开电磁阀，再开六通阀
            m_comm->setValveBit(0, true);
            m_comm->setSixWayValve1(true);

            // 通知界面更新图形状态
            emit deviceStateChanged("电磁阀", true);
            emit deviceStateChanged("六通阀", true);

            emit logMessage("自动流程",
                            QString("六通阀开启：先电磁阀 后六通阀 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
        } else {
            // 先关电磁阀，再关六通阀
            m_comm->setValveBit(0, false);
            m_comm->setSixWayValve1(false);

            emit deviceStateChanged("电磁阀", false);
            emit deviceStateChanged("六通阀", false);

            emit logMessage("自动流程",
                            QString("六通阀关闭：先电磁阀 后六通阀 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
        }
        break;

    case AutoEventType::Polarity:
        if (e.onAction) {
            emit polarityOnTriggered();
            emit logMessage("自动流程",
                            QString("极性反转开启 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
        } else {
            emit polarityOffTriggered();
            emit logMessage("自动流程",
                            QString("极性反转关闭 (t=%1 min)").arg(e.timeMin, 0, 'f', 3));
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
        settings.setValue("type", static_cast<int>(m_groups[i].type));
        settings.setValue("onTime", m_groups[i].onTimeMin);
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
        g.type = static_cast<AutoEventType>(settings.value("type", 0).toInt());
        g.onTimeMin = settings.value("onTime", 0.0).toDouble();
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
        obj["type"] = static_cast<int>(g.type);
        obj["onTimeMin"] = g.onTimeMin;
        obj["offTimeMin"] = g.offTimeMin;
        arr.append(obj);
    }
    root["version"] = 1;
    root["groups"] = arr;

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
    QJsonArray arr = root["groups"].toArray();

    QList<AutoEventGroup> newGroups;
    for (const auto &v : arr) {
        QJsonObject obj = v.toObject();
        AutoEventGroup g;
        g.type = static_cast<AutoEventType>(obj["type"].toInt());
        g.onTimeMin = obj["onTimeMin"].toDouble();
        g.offTimeMin = obj["offTimeMin"].toDouble();
        newGroups.append(g);
    }

    m_groups = newGroups;
    saveToSettings();
    return true;
}