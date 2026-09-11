#include "communication.h"
#include <QModbusReply>
#include <QDateTime>
#include <QDebug>

Communication::Communication(QObject *parent)
    : QObject(parent)
{
    m_fastTimer = new QTimer(this);
    m_fastTimer->setInterval(20);
    m_fastTimer->setTimerType(Qt::PreciseTimer);

    m_slowTimer = new QTimer(this);
    m_slowTimer->setInterval(1000);

    connect(m_fastTimer, &QTimer::timeout, this, &Communication::onFastTimer);
    connect(m_slowTimer, &QTimer::timeout, this, &Communication::onSlowTimer);
}

Communication::~Communication()
{
    stopPolling();
    disconnectDevice();
}

void Communication::setTcdPowered(bool powered)
{
    m_tcdPowered = powered;
}

void Communication::connectToDevice(const QString &ip, quint16 port)
{
    if (m_client) {
        m_client->disconnectDevice();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_client = new QModbusTcpClient(this);
    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_client->setTimeout(2000);
    m_client->setNumberOfRetries(0);

    connect(m_client, &QModbusTcpClient::stateChanged, this, &Communication::onStateChanged);
    connect(m_client, &QModbusTcpClient::errorOccurred, this, &Communication::onErrorOccurred);

    bool ok = m_client->connectDevice();
    if (ok)
        emit statusMessage("Modbus-TCP 连接中...");
    else
        emit statusMessage("Modbus-TCP 连接失败");
}

void Communication::disconnectDevice()
{
    m_fastTimer->stop();
    m_slowTimer->stop();

    if (m_client) {
        m_client->disconnectDevice();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_connected = false;
    m_busy = false;
    m_queue.clear();

    emit disconnected();
}

void Communication::startPolling()
{
    if (m_connected) {
        m_fastTimer->start();
        m_slowTimer->start();
        emit statusMessage("开始轮询数据");
    }
}

void Communication::stopPolling()
{
    m_fastTimer->stop();
    m_slowTimer->stop();
}

//-------------------------------------------------------------
// 定时器：入队
//-------------------------------------------------------------
void Communication::onFastTimer()
{
    if (!m_tcdPowered || !m_connected)
        return;

    if (m_busy && m_current.type == Request::FastPoll)
        return;
    for (const auto &r : m_queue) {
        if (r.type == Request::FastPoll)
            return;
    }

    Request req;
    req.type = Request::FastPoll;
    req.address = 0x0004;
    req.count = 3;
    enqueue(req);

    m_lastFastPollTime = QDateTime::currentMSecsSinceEpoch();
}

void Communication::onSlowTimer()
{
    if (!m_connected)
        return;

    if (m_busy && m_current.type == Request::SlowPoll)
        return;
    for (const auto &r : m_queue) {
        if (r.type == Request::SlowPoll)
            return;
    }

    if (m_tcdPowered) {
        // TCD 开启：读 0x0000~0x0003 和 0x0007~0x0009
        Request r1;
        r1.type = Request::SlowPoll;
        r1.address = 0x0000;
        r1.count = 4;
        enqueue(r1);

        Request r2;
        r2.type = Request::SlowPoll;
        r2.address = 0x0007;
        r2.count = 3;
        enqueue(r2);
    } else {
        // TCD 关闭：只读 0x0000（柱温箱）和 0x0003（压力）
        Request r1;
        r1.type = Request::SlowPoll;
        r1.address = 0x0000;
        r1.count = 1;
        enqueue(r1);

        Request r2;
        r2.type = Request::SlowPoll;
        r2.address = 0x0003;
        r2.count = 1;
        enqueue(r2);
    }
}

//-------------------------------------------------------------
// 队列管理
//-------------------------------------------------------------
int Communication::priorityOf(Request::Type t) const
{
    switch (t) {
    case Request::Write:         return 1;
    case Request::WriteMultiple: return 1;
    case Request::Read:          return 2;
    case Request::ReadRange:     return 2;
    case Request::FastPoll:      return 3;
    case Request::SlowPoll:      return 4;
    }
    return 5;
}

void Communication::enqueue(const Request &req)
{
    int prio = priorityOf(req.type);
    int insertPos = m_queue.size();
    for (int i = 0; i < m_queue.size(); ++i) {
        if (priorityOf(m_queue[i].type) > prio) {
            insertPos = i;
            break;
        }
    }
    m_queue.insert(insertPos, req);
    processQueue();
}

void Communication::processQueue()
{
    if (m_busy || !m_connected)
        return;
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState)
        return;

    // 追赶机制：队列空且快速轮询已逾期
    if (m_queue.isEmpty() && m_tcdPowered) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastFastPollTime == 0 || now - m_lastFastPollTime >= 20) {
            Request req;
            req.type = Request::FastPoll;
            req.address = 0x0004;
            req.count = 3;
            m_queue.append(req);
            m_lastFastPollTime = now;
        }
    }

    if (m_queue.isEmpty())
        return;

    m_busy = true;
    m_current = m_queue.takeFirst();
    sendRequest(m_current);
}

void Communication::sendRequest(const Request &req)
{
    if (req.type == Request::Write || req.type == Request::WriteMultiple) {
        QModbusDataUnit unit;
        if (req.type == Request::Write) {
            unit = QModbusDataUnit(QModbusDataUnit::HoldingRegisters, req.address, 1);
            unit.setValue(0, req.value);
        } else {
            unit = QModbusDataUnit(QModbusDataUnit::HoldingRegisters, req.address, req.values.size());
            for (int i = 0; i < req.values.size(); ++i)
                unit.setValue(i, req.values[i]);
        }

        emit logPacket("发送", QString("%1 [寄存器 0x%2 数量 %3]")
                                   .arg(req.description)
                                   .arg(req.address, 4, 16, QChar('0'))
                                   .arg(unit.valueCount()));

        QModbusReply *reply = m_client->sendWriteRequest(unit, 1);
        if (!reply) {
            emit logPacket("接收", QString("%1 写入失败: 发送请求失败").arg(req.description));
            m_busy = false;
            processQueue();
            return;
        }

        connect(reply, &QModbusReply::finished, this, [this, reply, desc = req.description]() {
            reply->deleteLater();
            if (reply->error() == QModbusDevice::NoError) {
                emit logPacket("接收", QString("%1 写入成功").arg(desc));
            } else {
                emit logPacket("接收", QString("%1 写入失败: %2").arg(desc, reply->errorString()));
            }
            m_busy = false;
            processQueue();
        });
    } else {
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, req.address, req.count);
        QModbusReply *reply = m_client->sendReadRequest(unit, 1);
        if (!reply) {
            if (req.type == Request::Read && req.readCallback)
                req.readCallback(0);
            if (req.type == Request::ReadRange && req.readRangeCallback)
                req.readRangeCallback(QVector<quint16>());
            m_busy = false;
            processQueue();
            return;
        }

        connect(reply, &QModbusReply::finished, this, [this, reply, req]() {
            reply->deleteLater();
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit result = reply->result();
                if (req.type == Request::FastPoll) {
                    processFastResponse(result);
                    emit fastDataUpdated();
                } else if (req.type == Request::SlowPoll) {
                    processSlowResponse(result, req.address);
                    emit slowDataUpdated();
                } else if (req.type == Request::Read) {
                    quint16 value = 0;
                    if (result.valueCount() > 0)
                        value = result.value(0);
                    if (req.readCallback) req.readCallback(value);
                } else if (req.type == Request::ReadRange) {
                    QVector<quint16> values;
                    values.reserve(result.valueCount());
                    for (int i = 0; i < result.valueCount(); ++i)
                        values.append(result.value(i));
                    if (req.readRangeCallback) req.readRangeCallback(values);
                }
            } else {
                if (req.type == Request::Read && req.readCallback)
                    req.readCallback(0);
                if (req.type == Request::ReadRange && req.readRangeCallback)
                    req.readRangeCallback(QVector<quint16>());
                if (req.type == Request::FastPoll)
                    emit statusMessage(QString("快速读取错误: %1").arg(reply->errorString()));
                if (req.type == Request::SlowPoll)
                    emit statusMessage(QString("慢速读取错误: %1").arg(reply->errorString()));
            }
            m_busy = false;
            processQueue();
        });
    }
}

//-------------------------------------------------------------
// 应答处理
//-------------------------------------------------------------
void Communication::processFastResponse(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 3)
        return;

    QMutexLocker locker(&m_mutex);
    m_tcdVoltageA  = static_cast<qint16>(unit.value(0));
    m_tcdVoltageB  = static_cast<qint16>(unit.value(1));
    m_tcdVoltageAB = static_cast<qint16>(unit.value(2));
}

void Communication::processSlowResponse(const QModbusDataUnit &unit, quint16 startAddr)
{
    QMutexLocker locker(&m_mutex);

    if (startAddr == 0x0000) {
        if (unit.valueCount() >= 1) m_columnOven1Temp = unit.value(0);
        if (unit.valueCount() >= 2) m_flow1 = unit.value(1);
        if (unit.valueCount() >= 3) m_flow2 = unit.value(2);
        if (unit.valueCount() >= 4) m_pressure = static_cast<qint16>(unit.value(3));
    } else if (startAddr == 0x0003) {
        if (unit.valueCount() >= 1) m_pressure = static_cast<qint16>(unit.value(0));
    } else if (startAddr == 0x0007) {
        if (unit.valueCount() >= 1) m_tcdPowerFA = static_cast<qint16>(unit.value(0));
        if (unit.valueCount() >= 2) m_tcdPowerFB = unit.value(1);
        if (unit.valueCount() >= 3) m_tcdMeasureTemp = unit.value(2);
    }
}

//-------------------------------------------------------------
// 单寄存器写
//-------------------------------------------------------------
void Communication::writeRegister(quint16 address, quint16 value, const QString &description)
{
    Request req;
    req.type = Request::Write;
    req.address = address;
    req.value = value;
    req.description = description;
    enqueue(req);
}

void Communication::setTcdTemperature(quint16 value) { writeRegister(0x03E8, value, "设置TCD温度"); }
void Communication::setLampPowerA(quint16 value) { writeRegister(0x03E9, value, "设置灯丝功率A"); }
void Communication::setLampPowerB(quint16 value) { writeRegister(0x03EA, value, "设置灯丝功率B"); }
void Communication::setChannelAVoltage(quint16 value) { writeRegister(0x03ED, value, "设置A通道电压"); }
void Communication::setChannelBVoltage(quint16 value) { writeRegister(0x03EE, value, "设置B通道电压"); }
void Communication::setChannelABVoltage(quint16 value) { writeRegister(0x03EF, value, "设置A-B通道电压"); }
void Communication::setZS(quint16 value) { writeRegister(0x03F0, value, "启动调节流程"); }
void Communication::setPrecision(quint16 value) { writeRegister(0x03F1, value, "设置最小精度"); }
void Communication::setRF(quint16 value) { writeRegister(0x03F2, value, "复位调节流程错误标志"); }
void Communication::setColumnOven1Temperature(quint16 value) { writeRegister(0x03FF, value, "设置柱温箱1温度"); }
void Communication::setColumnOven2Temperature(quint16 value) { writeRegister(0x03FF, value, "设置柱温箱2温度"); }
void Communication::setColumnOvenEnable(bool enable) { writeRegister(0x0400, enable ? 1 : 0, enable ? "开启柱温箱" : "关闭柱温箱"); }
void Communication::setSixWayValve1(bool on) { writeRegister(0x0404, on ? 1 : 0, on ? "六通阀1开启" : "六通阀1关闭"); }
void Communication::setSixWayValve2(bool on) { writeRegister(0x0404, on ? 2 : 0, on ? "六通阀2开启" : "六通阀2关闭"); }
void Communication::setFlow1Voltage(quint16 value) { writeRegister(0x0402, value, "设置流量控制器1电压"); }
void Communication::setFlow2Voltage(quint16 value) { writeRegister(0x0403, value, "设置流量控制器2电压"); }

void Communication::setValveBit(int valveIndex, bool on)
{
    if (valveIndex < 0 || valveIndex > 10) return;
    if (on) m_valveState |= (1 << valveIndex);
    else m_valveState &= ~(1 << valveIndex);
    writeRegister(0x0401, m_valveState,
                  QString("设置电磁阀 NV%1 %2").arg(valveIndex+1).arg(on ? "开启" : "关闭"));
}

void Communication::setAllValvesOff()
{
    m_valveState = 0;
    writeRegister(0x0401, m_valveState, "关闭所有电磁阀");
}

//-------------------------------------------------------------
// 批量写
//-------------------------------------------------------------
void Communication::writeMultipleRegisters(quint16 startAddr, const QVector<quint16> &values, const QString &description)
{
    if (values.isEmpty())
        return;

    if (startAddr <= 0x0401 && 0x0401 < startAddr + values.size()) {
        int idx = 0x0401 - startAddr;
        m_valveState = values[idx];
    }

    Request req;
    req.type = Request::WriteMultiple;
    req.address = startAddr;
    req.values = values;
    req.description = description;
    enqueue(req);
}

//-------------------------------------------------------------
// 按需读取
//-------------------------------------------------------------
void Communication::requestRegisterRead(quint16 address, std::function<void(quint16)> callback)
{
    Request req;
    req.type = Request::Read;
    req.address = address;
    req.count = 1;
    req.readCallback = callback;
    enqueue(req);
}

void Communication::requestRegisterReadRange(quint16 startAddr, quint16 count, std::function<void(const QVector<quint16>&)> callback)
{
    Request req;
    req.type = Request::ReadRange;
    req.address = startAddr;
    req.count = count;
    req.readRangeCallback = callback;
    enqueue(req);
}

//-------------------------------------------------------------
// 连接状态
//-------------------------------------------------------------
void Communication::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_connected = true;
        emit connected();
        emit statusMessage("Modbus-TCP 已连接");
    } else if (state == QModbusDevice::UnconnectedState) {
        m_connected = false;
        m_busy = false;
        m_queue.clear();
        emit disconnected();
        emit statusMessage("Modbus-TCP 已断开");
    }
}

void Communication::onErrorOccurred(QModbusDevice::Error error)
{
    emit statusMessage(QString("Modbus 错误: %1").arg(error));
}

bool Communication::isConnected() const
{
    return m_connected;
}

//-------------------------------------------------------------
// 数据 getter
//-------------------------------------------------------------
double Communication::columnOven1Temp() const { QMutexLocker locker(&m_mutex); return m_columnOven1Temp; }
double Communication::flow1() const { QMutexLocker locker(&m_mutex); return m_flow1; }
double Communication::flow2() const { QMutexLocker locker(&m_mutex); return m_flow2; }
double Communication::pressure() const { QMutexLocker locker(&m_mutex); return m_pressure; }
double Communication::tcdVoltageA() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageA; }
double Communication::tcdVoltageB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageB; }
double Communication::tcdVoltageAB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageAB; }
double Communication::tcdPowerFA() const { QMutexLocker locker(&m_mutex); return m_tcdPowerFA; }
double Communication::tcdPowerFB() const { QMutexLocker locker(&m_mutex); return m_tcdPowerFB; }
double Communication::tcdMeasureTemp() const { QMutexLocker locker(&m_mutex); return m_tcdMeasureTemp; }