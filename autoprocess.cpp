#include "autoprocess.h"
#include "communication.h"
#include <QDebug>
#include <QThread>

AutoProcessManager::AutoProcessManager(Communication *comm, QObject *parent)
    : QObject(parent)
    , m_comm(comm)
    , m_state(Idle)
    , m_processTimer(new QTimer(this))
    , m_stopSaveSent(false)
{
    m_processTimer->setInterval(3000);   // 3秒
    connect(m_processTimer, &QTimer::timeout, this, &AutoProcessManager::onProcessTimer);

    // 改为连接慢速数据更新，因为自动流程关注的是温度/流量
    connect(m_comm, &Communication::slowDataUpdated, this, &AutoProcessManager::onDataUpdated);
}

AutoProcessSettings AutoProcessManager::settings() const
{
    return m_settings;
}

void AutoProcessManager::setSettings(const AutoProcessSettings &s)
{
    m_settings = s;
}

QString AutoProcessManager::currentStateName() const
{
    switch (m_state) {
    case Idle: return "空闲";
    case SettingVerify: return "设置验证中";
    case Heating: return "升温中";
    case Stabilizing: return "稳定中";
    case Testing: return "测试中";
    case CoolingDown: return "冷却中";
    case Shutdown: return "关闭验证中";
    }
    return QString();
}

void AutoProcessManager::start()
{
    if (m_state != Idle) {
        emit logMessage("自动流程", "当前不在空闲状态，无法开始");
        return;
    }

    if (!m_comm || !m_comm->isConnected()) {
        emit logMessage("自动流程", "通信未连接，无法开始");
        return;
    }

    m_verifyQueue.clear();
    m_valveOpenDone = false;
    m_valveCloseDone = false;
    m_recordStarted = false;
    m_stopSaveSent = false;

    sendInitialSettings();

    changeState(SettingVerify);
    m_processTimer->start();
    emit logMessage("自动流程", "开始发送初始设置，等待验证...");
}

void AutoProcessManager::stop()
{
    if (m_state == Idle) return;
    emit logMessage("自动流程", "用户请求停止，进入冷却流程");
    beginCooling();
}

void AutoProcessManager::reset()
{
    if (m_state == Idle) return;
    m_processTimer->stop();

    // 若正在记录，先停止保存
    if (m_recordStarted && !m_stopSaveSent) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
    }

    m_verifyQueue.clear();

    // 发送所有关闭命令，并加入验证队列
    m_comm->enableDetector(false);
    enqueueVerify("关闭检测器", 0x03EB, 0,
                  [this]() { m_comm->enableDetector(false); });

    m_comm->setColumnOvenEnable(false);
    enqueueVerify("关闭柱温箱", 0x0400, 0,
                  [this]() { m_comm->setColumnOvenEnable(false); });

    m_comm->setFlow1Setpoint(0);
    enqueueVerify("关闭流量1", 0x0402, 0,
                  [this]() { m_comm->setFlow1Setpoint(0); });

    m_comm->setFlow2Setpoint(0);
    enqueueVerify("关闭流量2", 0x0403, 0,
                  [this]() { m_comm->setFlow2Setpoint(0); });

    m_comm->setSixWayValve1(false);
    m_comm->setSixWayValve2(false);
    enqueueVerify("关闭六通阀", 0x0404, 0,
                  [this]() { m_comm->setSixWayValve1(false); });

    emit deviceStateChanged("六通阀", false);
    emit deviceStateChanged("TCD", false);

    changeState(Shutdown);
    m_processTimer->start();
    emit logMessage("自动流程", "系统复位，正在验证关闭...");
}

void AutoProcessManager::onDataUpdated()
{
    // 数据更新信号暂不处理
}

void AutoProcessManager::onProcessTimer()
{
    // 优先处理验证队列
    if (!m_verifyQueue.isEmpty()) {
        processVerifyQueue();
        return;
    }

    switch (m_state) {
    case Idle:
        break;

    case SettingVerify:
        // 所有设置验证完成后，在 processVerifyQueue 中已处理进入 Heating，此处不会到达
        break;

    case Heating:
    case Stabilizing:
    {
        double ovenTemp = m_comm->columnOven1Temp();
        double tcdTemp = m_comm->tcdTemperature();
        bool ovenOk = qAbs(ovenTemp - m_settings.columnOvenTemp) <= m_settings.tempTolerance;
        bool tcdOk = qAbs(tcdTemp - m_settings.tcdTemp) <= m_settings.tempTolerance;

        if (m_state == Heating && ovenOk && tcdOk) {
            m_stabilizeStartTime = QDateTime::currentDateTime();
            changeState(Stabilizing);
            emit logMessage("自动流程", "温度已进入稳定范围，等待稳定...");
        } else if (m_state == Stabilizing) {
            if (m_stabilizeStartTime.msecsTo(QDateTime::currentDateTime()) >= 5000) {
                if (ovenOk && tcdOk) {
                    changeState(Testing);
                    m_testStartTime = QDateTime::currentDateTime();
                    m_recordStarted = false;
                    m_valveOpenDone = false;
                    m_valveCloseDone = false;
                    emit logMessage("自动流程", "温度稳定，开始测试流程");
                } else {
                    changeState(Heating);
                }
            }
        }
        break;
    }

    case Testing:
    {
        double elapsedMin = m_testStartTime.msecsTo(QDateTime::currentDateTime()) / 60000.0;
        sendTestSequenceCommand(elapsedMin);
        if (elapsedMin >= m_settings.stopRecordMin) {
            emit logMessage("自动流程", "测试流程完成，开始冷却");
            beginCooling();
        }
        break;
    }

    case CoolingDown:
    {
        double ovenTemp = m_comm->columnOven1Temp();
        emit logMessage("自动流程", QString("冷却中，当前柱温箱温度: %1 ℃").arg(ovenTemp));
        if (ovenTemp <= m_settings.coolDownTemp) {
            reset();   // 进入关闭验证
        }
        break;
    }

    case Shutdown:
        // 关闭验证完成后已在 processVerifyQueue 中回到 Idle
        break;
    }
}

void AutoProcessManager::sendInitialSettings()
{
    m_comm->setColumnOvenEnable(true);
    enqueueVerify("开启柱温箱", 0x0400, 1,
                  [this]() { m_comm->setColumnOvenEnable(true); });
    m_comm->setColumnOven1Temperature(static_cast<quint16>(m_settings.columnOvenTemp));
    enqueueVerify("柱温箱温度", 0x03FF, m_settings.columnOvenTemp,
                  [this]() { m_comm->setColumnOven1Temperature(m_settings.columnOvenTemp); });
    m_comm->setTcdTemperature(static_cast<quint16>(m_settings.tcdTemp));
    enqueueVerify("TCD温度", 0x03E8, m_settings.tcdTemp,
                  [this]() { m_comm->setTcdTemperature(m_settings.tcdTemp); });
    m_comm->setFlow1Setpoint(static_cast<quint16>(m_settings.carrierFlow1));
    enqueueVerify("流量1", 0x0402, m_settings.carrierFlow1,
                  [this]() { m_comm->setFlow1Setpoint(m_settings.carrierFlow1); });
    m_comm->setFlow2Setpoint(static_cast<quint16>(m_settings.carrierFlow2));
    enqueueVerify("流量2", 0x0403, m_settings.carrierFlow2,
                  [this]() { m_comm->setFlow2Setpoint(m_settings.carrierFlow2); });
    m_comm->enableDetector(true);
    enqueueVerify("开启检测器", 0x03EB, 1,
                  [this]() { m_comm->enableDetector(true); });
    emit deviceStateChanged("TCD", true);

    emit logMessage("自动流程", "初始设置命令已发出，等待验证...");
}

void AutoProcessManager::sendTestSequenceCommand(double elapsedMin)
{
    // 开始记录
    if (!m_recordStarted && elapsedMin >= m_settings.startRecordMin) {
        m_recordStarted = true;
        emit saveDataTriggered();
        emit logMessage("自动流程", "开始记录TCD数据");
    }

    // 停止记录（只触发一次）
    if (m_recordStarted && !m_stopSaveSent && elapsedMin >= m_settings.stopRecordMin) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
        emit logMessage("自动流程", "停止记录TCD数据");
    }

    // 六通阀开启（只执行一次）
    if (!m_valveOpenDone && elapsedMin >= m_settings.valveOpenMin) {
        m_valveOpenDone = true;
        m_comm->setSixWayValve1(true);
        enqueueVerify("开启六通阀", 0x0404, 1,
                      [this]() { m_comm->setSixWayValve1(true); });
        emit deviceStateChanged("六通阀", true);
        emit logMessage("自动流程", "六通阀开启");
    }

    // 六通阀关闭（只执行一次）
    if (m_valveOpenDone && !m_valveCloseDone && elapsedMin >= m_settings.valveCloseMin) {
        m_valveCloseDone = true;
        m_comm->setSixWayValve1(false);
        enqueueVerify("关闭六通阀", 0x0404, 0,
                      [this]() { m_comm->setSixWayValve1(false); });
        emit deviceStateChanged("六通阀", false);
        emit logMessage("自动流程", "六通阀关闭");
    }
}

void AutoProcessManager::beginCooling()
{
    // 若正在记录，先停止保存
    if (m_recordStarted && !m_stopSaveSent) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
    }

    m_verifyQueue.clear();   // 清空可能残留的验证队列
    m_comm->enableDetector(false);
    enqueueVerify("关闭检测器", 0x03EB, 0,
                  [this]() { m_comm->enableDetector(false); });
    m_comm->setColumnOven1Temperature(static_cast<quint16>(m_settings.coolDownTemp));
    enqueueVerify("设置柱温箱冷却温度", 0x03FF, m_settings.coolDownTemp,
                  [this]() { m_comm->setColumnOven1Temperature(m_settings.coolDownTemp); });
    m_comm->setColumnOvenEnable(true);   // 保持开启以降温
    enqueueVerify("保持柱温箱开启", 0x0400, 1,
                  [this]() { m_comm->setColumnOvenEnable(true); });
    emit deviceStateChanged("TCD", false);

    changeState(CoolingDown);
    emit logMessage("自动流程", "进入冷却阶段，目标温度 " + QString::number(m_settings.coolDownTemp) + " ℃");
}

void AutoProcessManager::enqueueVerify(const QString &name, quint16 address, quint16 expected,
                                       std::function<void()> resend)
{
    CommandToVerify cmd;
    cmd.name = name;
    cmd.address = address;
    cmd.expected = expected;
    cmd.resend = resend;
    cmd.retryCount = 0;
    m_verifyQueue.append(cmd);
}

void AutoProcessManager::processVerifyQueue()
{
    // 如果队列为空或已有读取请求在途，则本次不处理
    if (m_verifyQueue.isEmpty() || m_verifyReadPending)
        return;

    CommandToVerify cmd = m_verifyQueue.first();
    QString name = cmd.name;

    // 标记读取进行中，防止并发
    m_verifyReadPending = true;

    m_comm->requestRegisterRead(cmd.address, [this, name](quint16 actual) {
        // 读取完成，清除标记
        m_verifyReadPending = false;

        int idx = -1;
        for (int i = 0; i < m_verifyQueue.size(); ++i) {
            if (m_verifyQueue[i].name == name) {
                idx = i;
                break;
            }
        }

        if (idx == -1)
            return;

        CommandToVerify &cmdRef = m_verifyQueue[idx];

        if (actual == cmdRef.expected) {
            m_verifyQueue.removeAt(idx);
            emit logMessage("自动流程", QString("%1 成功").arg(name));
        } else {
            if (cmdRef.retryCount < 3) {
                cmdRef.retryCount++;
                cmdRef.resend();
                emit logMessage("自动流程", QString("%1 失败，重试 (%2/3)").arg(name).arg(cmdRef.retryCount));
            } else {
                m_verifyQueue.removeAt(idx);
                emit logMessage("自动流程", QString("%1 失败，超过最大重试次数").arg(name));
            }
        }

        // 队列已空，根据当前状态进行切换
        if (m_verifyQueue.isEmpty()) {
            if (m_state == SettingVerify) {
                changeState(Heating);
                emit logMessage("自动流程", "设置验证通过，开始升温");
            } else if (m_state == Shutdown) {
                m_processTimer->stop();
                changeState(Idle);
                emit logMessage("自动流程", "关闭验证通过，系统已复位");
                emit finished();
            }
        }
    });
}

void AutoProcessManager::changeState(State newState)
{
    m_state = newState;
    emit stateChanged(currentStateName());
}