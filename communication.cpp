#include "communication.h"
#include <QModbusReply>
#include <QDateTime>
#include <QDebug>
#include <QThread>

Communication::Communication(QObject *parent)
    : QObject(parent)
    , m_fastClient(nullptr)
    , m_slowClient(nullptr)
    , m_fastTimer(new QTimer(this))
    , m_slowTimer(new QTimer(this))
    , m_fastConnected(false)
    , m_slowConnected(false)
    , m_slowBusy(false)
    , m_columnOven1Temp(0)
    , m_flow1(0)
    , m_flow2(0)
    , m_pressure(0)
    , m_tcdVoltageA(0)
    , m_tcdVoltageB(0)
    , m_tcdVoltageAB(0)
    , m_tcdTemperature(0)
    , m_valveState(0)
    , m_tcdPowered(false)
{
    // 快速定时器：精确定时器类型
    m_fastTimer->setInterval(40);
    m_fastTimer->setTimerType(Qt::PreciseTimer);
    m_slowTimer->setInterval(1000);  // 慢速保持不变

    connect(m_fastTimer, &QTimer::timeout, this, &Communication::pollFastData);
    connect(m_slowTimer, &QTimer::timeout, this, &Communication::pollSlowData);
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
    // 创建快速客户端
    if (m_fastClient) {
        m_fastClient->disconnectDevice();
        m_fastClient->deleteLater();
    }
    m_fastClient = new QModbusTcpClient(this);
    m_fastClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_fastClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_fastClient->setTimeout(500);
    m_fastClient->setNumberOfRetries(2);
    connect(m_fastClient, &QModbusTcpClient::stateChanged, this, &Communication::onFastStateChanged);
    connect(m_fastClient, &QModbusTcpClient::errorOccurred, this, &Communication::onFastErrorOccurred);

    // 创建慢速客户端
    if (m_slowClient) {
        m_slowClient->disconnectDevice();
        m_slowClient->deleteLater();
    }
    m_slowClient = new QModbusTcpClient(this);
    m_slowClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_slowClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_slowClient->setTimeout(2000);   // 慢速客户端给予更长超时
    m_slowClient->setNumberOfRetries(1);
    connect(m_slowClient, &QModbusTcpClient::stateChanged, this, &Communication::onSlowStateChanged);
    connect(m_slowClient, &QModbusTcpClient::errorOccurred, this, &Communication::onSlowErrorOccurred);

    // 发起连接（异步）
    m_fastConnected = m_fastClient->connectDevice();
    m_slowConnected = m_slowClient->connectDevice();

    if (m_fastConnected && m_slowConnected)
        emit statusMessage("Modbus-TCP 双客户端连接成功");
    else
        emit statusMessage("Modbus-TCP 连接中...");
}

void Communication::disconnectDevice()
{
    if (m_fastClient) {
        m_fastClient->disconnectDevice();
        m_fastClient->deleteLater();
        m_fastClient = nullptr;
    }
    if (m_slowClient) {
        m_slowClient->disconnectDevice();
        m_slowClient->deleteLater();
        m_slowClient = nullptr;
    }
    m_fastConnected = false;
    m_slowConnected = false;
    m_slowQueue.clear();
    m_slowBusy = false;
    emit disconnected();
}

void Communication::startPolling()
{
    if (m_fastConnected)
        m_fastTimer->start();
    if (m_slowConnected)
        m_slowTimer->start();
    emit statusMessage("开始轮询数据");
}

void Communication::stopPolling()
{
    m_fastTimer->stop();
    m_slowTimer->stop();
}

//-------------------------------------------------------------
// 快速轮询
//-------------------------------------------------------------
void Communication::pollFastData()
{
    // TCD 电源关闭时，不发送快速轮询请求
    if (!m_tcdPowered)
        return;

    if (!m_fastClient || !m_fastConnected)
        return;
    if (m_fastClient->state() != QModbusDevice::ConnectedState)
        return;

    QModbusDataUnit unit = fastReadRequest();
    QModbusReply *reply = m_fastClient->sendReadRequest(unit, 1);
    if (!reply) {
        emit statusMessage("快速读取请求发送失败");
        return;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QModbusDevice::NoError) {
            processFastResponse(reply->result());
            emit fastDataUpdated();
        } else {
            emit statusMessage(QString("快速读取错误: %1").arg(reply->errorString()));
        }
    });
}

QModbusDataUnit Communication::fastReadRequest() const
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 4, 3);
    return unit;
}

void Communication::processFastResponse(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 3)
        return;

    QMutexLocker locker(&m_mutex);
    m_tcdVoltageA  = static_cast<qint16>(unit.value(0));
    m_tcdVoltageB  = static_cast<qint16>(unit.value(1));
    m_tcdVoltageAB = static_cast<qint16>(unit.value(2));
}

void Communication::pollSlowData()
{
    if (!m_slowClient || !m_slowConnected)
        return;
    if (m_slowClient->state() != QModbusDevice::ConnectedState)
        return;

    if (m_tcdPowered) {
        // TCD 电源开启：读取 0x0000~0x0003（柱温箱、流量1、流量2、压力）
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 4);
        QModbusReply *reply = m_slowClient->sendReadRequest(unit, 1);
        if (!reply) {
            emit statusMessage("慢速读取请求发送失败");
            return;
        }

        connect(reply, &QModbusReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() == QModbusDevice::NoError) {
                processSlowResponse(reply->result());

                // 继续读取 TCD 温度（0x0007）
                QModbusDataUnit tcdUnit(QModbusDataUnit::HoldingRegisters, 0x0007, 1);
                QModbusReply *tcdReply = m_slowClient->sendReadRequest(tcdUnit, 1);
                if (!tcdReply) {
                    emit statusMessage("慢速读取TCD温度请求发送失败");
                    emit slowDataUpdated();
                    return;
                }

                connect(tcdReply, &QModbusReply::finished, this, [this, tcdReply]() {
                    tcdReply->deleteLater();
                    if (tcdReply->error() == QModbusDevice::NoError) {
                        const QModbusDataUnit result = tcdReply->result();
                        if (result.valueCount() > 0) {
                            QMutexLocker locker(&m_mutex);
                            m_tcdTemperature = static_cast<qint16>(result.value(0));
                        }
                    } else {
                        emit statusMessage(QString("慢速读取TCD温度错误: %1").arg(tcdReply->errorString()));
                    }
                    emit slowDataUpdated();
                });
            } else {
                emit statusMessage(QString("慢速读取错误: %1").arg(reply->errorString()));
            }
        });
    } else {
        // TCD 电源关闭：只读取柱温箱（0x0000）和压力（0x0003）
        // 先读取柱温箱
        QModbusDataUnit ovenUnit(QModbusDataUnit::HoldingRegisters, 0x0000, 1);
        QModbusReply *ovenReply = m_slowClient->sendReadRequest(ovenUnit, 1);
        if (!ovenReply) {
            emit statusMessage("读取柱温箱请求发送失败");
            return;
        }

        connect(ovenReply, &QModbusReply::finished, this, [this, ovenReply]() {
            ovenReply->deleteLater();
            if (ovenReply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit result = ovenReply->result();
                if (result.valueCount() > 0) {
                    QMutexLocker locker(&m_mutex);
                    m_columnOven1Temp = result.value(0);
                }
            } else {
                emit statusMessage(QString("读取柱温箱错误: %1").arg(ovenReply->errorString()));
            }

            // 再读取压力
            QModbusDataUnit pressUnit(QModbusDataUnit::HoldingRegisters, 0x0003, 1);
            QModbusReply *pressReply = m_slowClient->sendReadRequest(pressUnit, 1);
            if (!pressReply) {
                emit statusMessage("读取压力请求发送失败");
                emit slowDataUpdated();
                return;
            }

            connect(pressReply, &QModbusReply::finished, this, [this, pressReply]() {
                pressReply->deleteLater();
                if (pressReply->error() == QModbusDevice::NoError) {
                    const QModbusDataUnit result = pressReply->result();
                    if (result.valueCount() > 0) {
                        QMutexLocker locker(&m_mutex);
                        m_pressure = static_cast<qint16>(result.value(0));
                    }
                } else {
                    emit statusMessage(QString("读取压力错误: %1").arg(pressReply->errorString()));
                }
                emit slowDataUpdated();
            });
        });
    }
}

QModbusDataUnit Communication::slowReadRequest() const
{
    // 只读取非 TCD 参数：柱温箱温度、流量1、流量2、压力
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 4);
    return unit;
}

void Communication::processSlowResponse(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 4)
        return;

    QMutexLocker locker(&m_mutex);
    m_columnOven1Temp = unit.value(0);
    m_flow1 = unit.value(1);
    m_flow2 = unit.value(2);
    m_pressure = static_cast<qint16>(unit.value(3));
    // TCD 温度已由单独的读取更新
}

//-------------------------------------------------------------
// 慢速请求队列（写入与按需读取）
//-------------------------------------------------------------
void Communication::enqueueSlowRead(quint16 address, std::function<void(quint16)> callback)
{
    if (!m_slowClient || !m_slowConnected) {
        callback(0);
        return;
    }

    SlowRequest req;
    req.type = SlowRequest::Read;
    req.address = address;
    req.readCallback = callback;
    m_slowQueue.enqueue(req);

    processSlowQueue();
}

void Communication::enqueueSlowWrite(quint16 address, quint16 value, const QString &description)
{
    if (!m_slowClient || !m_slowConnected)
        return;

    SlowRequest req;
    req.type = SlowRequest::Write;
    req.address = address;
    req.value = value;
    req.description = description;
    m_slowQueue.enqueue(req);

    processSlowQueue();
}

void Communication::processSlowQueue()
{
    if (m_slowBusy || m_slowQueue.isEmpty())
        return;

    m_slowBusy = true;
    SlowRequest req = m_slowQueue.dequeue();

    if (req.type == SlowRequest::Read) {

        qDebug() << "[慢速队列发送] 时间:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
            << " 地址:0x" << QString::number(req.address,16)
            << " 队列剩余:" << m_slowQueue.size();
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, req.address, 1);
        QModbusReply *reply = m_slowClient->sendReadRequest(unit, 1);
        if (!reply) {
            if (req.readCallback) req.readCallback(0);
            m_slowBusy = false;
            processSlowQueue();
            return;
        }

        connect(reply, &QModbusReply::finished, this, [this, reply, callback = req.readCallback]() {
            reply->deleteLater();
            quint16 value = 0;
            if (reply->error() == QModbusDevice::NoError) {
                const QModbusDataUnit result = reply->result();
                if (result.valueCount() > 0)
                    value = result.value(0);
            }
            if (callback) callback(value);
            m_slowBusy = false;
            processSlowQueue();
        });
    } else { // Write
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, req.address, 1);
        unit.setValue(0, req.value);

        emit logPacket("发送", QString("%1 [寄存器 0x%2 = %3]")
                                   .arg(req.description)
                                   .arg(req.address, 4, 16, QChar('0'))
                                   .arg(req.value));

        QModbusReply *reply = m_slowClient->sendWriteRequest(unit, 1);
        if (!reply) {
            emit logPacket("接收", QString("%1 写入失败: 发送请求失败").arg(req.description));
            m_slowBusy = false;
            processSlowQueue();
            return;
        }

        connect(reply, &QModbusReply::finished, this, [this, reply, description = req.description]() {
            if (reply->error() == QModbusDevice::NoError) {
                emit logPacket("接收", QString("%1 写入成功").arg(description));
            } else {
                emit logPacket("接收", QString("%1 写入失败: %2").arg(description, reply->errorString()));
            }
            reply->deleteLater();
            m_slowBusy = false;
            processSlowQueue();
        });
    }
}

//-------------------------------------------------------------
// 写寄存器入口（统一入队）
//-------------------------------------------------------------
void Communication::writeRegister(QModbusTcpClient *client, quint16 address, quint16 value, const QString &description)
{
    Q_UNUSED(client);
    enqueueSlowWrite(address, value, description);
}

// 命令实现（全部使用慢速队列）
void Communication::setTcdTemperature(quint16 value) { writeRegister(m_slowClient, 0x03E8, value, "设置TCD温度"); }
void Communication::setLampPowerA(quint16 value) { writeRegister(m_slowClient, 0x03E9, value, "设置灯丝功率A"); }
void Communication::setLampPowerB(quint16 value) { writeRegister(m_slowClient, 0x03EA, value, "设置灯丝功率B"); }
void Communication::enableDetector(bool enable) { writeRegister(m_slowClient, 0x03EB, enable ? 1 : 0, enable ? "开启检测器加热与灯丝供电" : "关闭检测器加热与灯丝供电"); }
void Communication::setChannelAVoltage(quint16 value) { writeRegister(m_slowClient, 0x03ED, value, "设置A通道电压"); }
void Communication::setChannelBVoltage(quint16 value) { writeRegister(m_slowClient, 0x03EE, value, "设置B通道电压"); }
void Communication::setChannelABVoltage(quint16 value) { writeRegister(m_slowClient, 0x03EF, value, "设置A-B通道电压"); }
void Communication::setPrecision(quint16 value) { writeRegister(m_slowClient, 0x03F1, value, "设置最小精度"); }
void Communication::setColumnOven1Temperature(quint16 value) { writeRegister(m_slowClient, 0x03FF, value, "设置柱温箱1温度"); }
void Communication::setColumnOven2Temperature(quint16 value) { writeRegister(m_slowClient, 0x03FF, value, "设置柱温箱2温度"); }
void Communication::setColumnOvenEnable(bool enable) { writeRegister(m_slowClient, 0x0400, enable ? 1 : 0, enable ? "开启柱温箱" : "关闭柱温箱"); }
void Communication::setSixWayValve1(bool on) { writeRegister(m_slowClient, 0x0404, on ? 1 : 0, on ? "六通阀1开启" : "六通阀1关闭"); }
void Communication::setSixWayValve2(bool on) { writeRegister(m_slowClient, 0x0404, on ? 2 : 0, on ? "六通阀2开启" : "六通阀2关闭"); }

void Communication::setValveBit(int valveIndex, bool on)
{
    if (valveIndex < 0 || valveIndex > 10) return;
    if (on) m_valveState |= (1 << valveIndex);
    else m_valveState &= ~(1 << valveIndex);
    writeRegister(m_slowClient, 0x0401, m_valveState, QString("设置电磁阀 NV%1 %2").arg(valveIndex+1).arg(on ? "开启" : "关闭"));
}

void Communication::setAllValvesOff()
{
    m_valveState = 0;
    writeRegister(m_slowClient, 0x0401, m_valveState, "关闭所有电磁阀");
}

void Communication::setFlow1Setpoint(quint16 value) { writeRegister(m_slowClient, 0x0402, value, "设置流量控制器1电压"); }
void Communication::setFlow2Setpoint(quint16 value) { writeRegister(m_slowClient, 0x0403, value, "设置流量控制器2电压"); }

//-------------------------------------------------------------
// 按需读取入口（统一入队）
//-------------------------------------------------------------
void Communication::requestRegisterRead(quint16 address, std::function<void(quint16)> callback)
{
    static qint64 lastReadTime = 0;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 interval = (lastReadTime == 0) ? 0 : (now - lastReadTime);
    lastReadTime = now;

    qDebug() << "[慢速读取入队] 时间:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << " 地址:0x" << QString::number(address,16)
             << " 距上次读取:" << interval << "ms"
             << " 当前队列长度:" << m_slowQueue.size();

    enqueueSlowRead(address, callback);
}

//-------------------------------------------------------------
// 状态处理
//-------------------------------------------------------------
void Communication::onFastStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_fastConnected = true;
        if (m_slowConnected) {
            emit connected();
            emit statusMessage("Modbus-TCP 快速通道已连接");
        }
    } else if (state == QModbusDevice::UnconnectedState) {
        m_fastConnected = false;
        if (!m_slowConnected) {
            emit disconnected();
            emit statusMessage("Modbus-TCP 快速通道已断开");
        }
    }
}

void Communication::onSlowStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_slowConnected = true;
        if (m_fastConnected) {
            emit connected();
            emit statusMessage("Modbus-TCP 慢速通道已连接");
        }
    } else if (state == QModbusDevice::UnconnectedState) {
        m_slowConnected = false;
        if (!m_fastConnected) {
            emit disconnected();
            emit statusMessage("Modbus-TCP 慢速通道已断开");
        }
    }
}

void Communication::onFastErrorOccurred(QModbusDevice::Error error)
{
    emit statusMessage(QString("快速客户端错误: %1").arg(error));
}

void Communication::onSlowErrorOccurred(QModbusDevice::Error error)
{
    emit statusMessage(QString("慢速客户端错误: %1").arg(error));
}

bool Communication::isConnected() const
{
    return m_fastConnected && m_slowConnected;
}

//-------------------------------------------------------------
// 数据 getter（线程安全）
//-------------------------------------------------------------
double Communication::columnOven1Temp() const { QMutexLocker locker(&m_mutex); return m_columnOven1Temp; }
double Communication::flow1() const { QMutexLocker locker(&m_mutex); return m_flow1; }
double Communication::flow2() const { QMutexLocker locker(&m_mutex); return m_flow2; }
double Communication::pressure() const { QMutexLocker locker(&m_mutex); return m_pressure; }
double Communication::tcdVoltageA() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageA; }
double Communication::tcdVoltageB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageB; }
double Communication::tcdVoltageAB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageAB; }
double Communication::tcdTemperature() const { QMutexLocker locker(&m_mutex); return m_tcdTemperature; }