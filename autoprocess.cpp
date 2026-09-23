#include "autoprocess.h"
#include "communication.h"
#include <QDebug>

AutoProcessManager::AutoProcessManager(Communication *comm, QObject *parent)
    : QObject(parent)
    , m_comm(comm)
    , m_state(Idle)
    , m_processTimer(new QTimer(this))
    , m_stopSaveSent(false)
{
    m_processTimer->setInterval(1500);
    connect(m_processTimer, &QTimer::timeout, this, &AutoProcessManager::onProcessTimer);
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

    if (m_recordStarted && !m_stopSaveSent) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
    }

    m_verifyQueue.clear();

    // 关闭柱温箱、电磁阀、流量1、流量2、六通阀（批量写 0x0400~0x0404）
    QVector<quint16> shutdownVals = {0, 0, 0, 0, 0};
    m_comm->writeMultipleRegisters(0x0400, shutdownVals, "复位: 关闭柱温箱/流量/阀门");
    enqueueVerifyRange("关闭柱温箱及流量阀门", 0x0400, shutdownVals,
                       [this, shutdownVals]() { m_comm->writeMultipleRegisters(0x0400, shutdownVals, "重发: 关闭柱温箱/流量/阀门"); });

    emit deviceStateChanged("电磁阀", false);
    emit deviceStateChanged("六通阀", false);

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
    if (!m_verifyQueue.isEmpty()) {
        processVerifyQueue();
        return;
    }

    switch (m_state) {
    case Idle:
        break;

    case SettingVerify:
        break;

    case Heating:
    case Stabilizing:
    {
        double ovenTemp = m_comm->columnOven1Temp();
        double tcdTemp = m_comm->tcdMeasureTemp();
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
            reset();
        }
        break;
    }

    case Shutdown:
        break;
    }
}

void AutoProcessManager::sendInitialSettings()
{
    // 批量写 0x03FF~0x0400：柱温箱温度 + 使能
    QVector<quint16> ovenVals = {
        static_cast<quint16>(m_settings.columnOvenTemp),
        1
    };
    m_comm->writeMultipleRegisters(0x03FF, ovenVals, "初始设置: 柱温箱温度+使能");
    enqueueVerifyRange("柱温箱设置", 0x03FF, ovenVals,
                       [this, ovenVals]() { m_comm->writeMultipleRegisters(0x03FF, ovenVals, "重发: 柱温箱温度+使能"); });

    // TCD 温度（单独写）
    quint16 tcdTemp = static_cast<quint16>(m_settings.tcdTemp);
    m_comm->setTcdTemperature(tcdTemp);
    enqueueVerify("TCD温度", 0x03E8, tcdTemp,
                  [this, tcdTemp]() { m_comm->setTcdTemperature(tcdTemp); });

    // 批量写 0x0402~0x0403：流量电压1 + 流量电压2
    QVector<quint16> flowVals = {
        static_cast<quint16>(m_settings.carrierFlow1Voltage),
        static_cast<quint16>(m_settings.carrierFlow2Voltage)
    };
    m_comm->writeMultipleRegisters(0x0402, flowVals, "初始设置: 流量电压1+流量电压2");
    enqueueVerifyRange("流量电压设置", 0x0402, flowVals,
                       [this, flowVals]() { m_comm->writeMultipleRegisters(0x0402, flowVals, "重发: 流量电压1+流量电压2"); });

    emit logMessage("自动流程", "初始设置命令已发出，等待验证...");
}

void AutoProcessManager::sendTestSequenceCommand(double elapsedMin)
{
    if (!m_recordStarted && elapsedMin >= m_settings.startRecordMin) {
        m_recordStarted = true;
        emit saveDataTriggered();
        emit logMessage("自动流程", "开始记录TCD数据");
    }

    if (m_recordStarted && !m_stopSaveSent && elapsedMin >= m_settings.stopRecordMin) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
        emit logMessage("自动流程", "停止记录TCD数据");
    }

    if (!m_valveOpenDone && elapsedMin >= m_settings.valveOpenMin) {
        m_valveOpenDone = true;
        // 先开启电磁阀（bit0），再开启六通阀
        m_comm->setValveBit(0, true);
        m_comm->setSixWayValve1(true);
        enqueueVerify("开启六通阀", 0x0404, 1,
                      [this]() { m_comm->setSixWayValve1(true); });
        emit deviceStateChanged("电磁阀", true);
        emit deviceStateChanged("六通阀", true);
        emit logMessage("自动流程", "六通阀开启");
    }

    if (m_valveOpenDone && !m_valveCloseDone && elapsedMin >= m_settings.valveCloseMin) {
        m_valveCloseDone = true;
        m_comm->setValveBit(0, false);
        m_comm->setSixWayValve1(false);
        enqueueVerify("关闭六通阀", 0x0404, 0,
                      [this]() { m_comm->setSixWayValve1(false); });
        emit deviceStateChanged("电磁阀", false);
        emit deviceStateChanged("六通阀", false);
        emit logMessage("自动流程", "六通阀关闭");
    }
}

void AutoProcessManager::beginCooling()
{
    if (m_recordStarted && !m_stopSaveSent) {
        m_stopSaveSent = true;
        emit stopDataSaveTriggered();
    }

    m_verifyQueue.clear();

    // 批量写 0x03FF~0x0400：柱温箱冷却温度 + 保持使能
    QVector<quint16> ovenVals = {
        static_cast<quint16>(m_settings.coolDownTemp),
        1
    };
    m_comm->writeMultipleRegisters(0x03FF, ovenVals, "冷却: 柱温箱温度+使能");
    enqueueVerifyRange("柱温箱冷却设置", 0x03FF, ovenVals,
                       [this, ovenVals]() { m_comm->writeMultipleRegisters(0x03FF, ovenVals, "重发: 冷却柱温箱温度+使能"); });

    changeState(CoolingDown);
    emit logMessage("自动流程", "进入冷却阶段，目标温度 " + QString::number(m_settings.coolDownTemp) + " ℃");
}

void AutoProcessManager::enqueueVerify(const QString &name, quint16 address, quint16 expected,
                                       std::function<void()> resend)
{
    CommandToVerify cmd;
    cmd.name = name;
    cmd.startAddr = address;
    cmd.expectedValues = QVector<quint16>{expected};
    cmd.resend = resend;
    cmd.retryCount = 0;
    m_verifyQueue.append(cmd);
}

void AutoProcessManager::enqueueVerifyRange(const QString &name, quint16 startAddr,
                                            const QVector<quint16> &expectedValues,
                                            std::function<void()> resend)
{
    CommandToVerify cmd;
    cmd.name = name;
    cmd.startAddr = startAddr;
    cmd.expectedValues = expectedValues;
    cmd.resend = resend;
    cmd.retryCount = 0;
    m_verifyQueue.append(cmd);
}

void AutoProcessManager::processVerifyQueue()
{
    if (m_verifyQueue.isEmpty() || m_verifyReadPending)
        return;

    m_verifyReadPending = true;
    CommandToVerify cmd = m_verifyQueue.first();
    QString name = cmd.name;
    quint16 startAddr = cmd.startAddr;
    quint16 count = static_cast<quint16>(cmd.expectedValues.size());

    // 打印验证读取请求（用户可见）
    emit logMessage("自动流程", QString("验证读取: %1 [寄存器 0x%2 数量 %3]")
                                    .arg(name)
                                    .arg(startAddr, 4, 16, QChar('0'))
                                    .arg(count));

    m_comm->requestRegisterReadRange(startAddr, count, [this, name](const QVector<quint16> &actual) {
        m_verifyReadPending = false;

        int idx = -1;
        for (int i = 0; i < m_verifyQueue.size(); ++i) {
            if (m_verifyQueue[i].name == name) {
                idx = i;
                break;
            }
        }
        if (idx == -1) return;

        CommandToVerify &cmdRef = m_verifyQueue[idx];

        bool allMatch = (actual.size() >= cmdRef.expectedValues.size());
        if (allMatch) {
            for (int i = 0; i < cmdRef.expectedValues.size(); ++i) {
                if (actual[i] != cmdRef.expectedValues[i]) {
                    allMatch = false;
                    break;
                }
            }
        }

        if (allMatch) {
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