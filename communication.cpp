#include "communication.h"
#include <QModbusReply>
#include <QDateTime>
#include <QDebug>

Communication::Communication(QObject *parent)
    : QObject(parent)
    , m_modbusClient(nullptr)
    , m_pollTimer(new QTimer(this))
    , m_connected(false)
    , m_columnOven1Temp(0)
    , m_flow1(0)
    , m_flow2(0)
    , m_pressure(0)
    , m_tcdVoltageA(0)
    , m_tcdVoltageB(0)
    , m_tcdVoltageAB(0)
    , m_tcdTemperature(0)
    , m_valveState(0)
{
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout, this, &Communication::pollData);
}

Communication::~Communication()
{
    stopPolling();
    disconnectDevice();
}

void Communication::connectToDevice(const QString &ip, quint16 port)
{
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
        m_modbusClient->deleteLater();
    }

    m_modbusClient = new QModbusTcpClient(this);
    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkAddressParameter, ip);
    m_modbusClient->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_modbusClient->setTimeout(1000);
    m_modbusClient->setNumberOfRetries(2);

    connect(m_modbusClient, &QModbusTcpClient::stateChanged, this, &Communication::onStateChanged);
    connect(m_modbusClient, &QModbusTcpClient::errorOccurred, this, &Communication::onErrorOccurred);

    m_connected = m_modbusClient->connectDevice();
    if (m_connected)
        emit statusMessage("Modbus-TCP 连接成功");
    else
        emit statusMessage("Modbus-TCP 连接失败");
}

void Communication::disconnectDevice()
{
    if (m_modbusClient) {
        m_modbusClient->disconnectDevice();
        m_modbusClient->deleteLater();
        m_modbusClient = nullptr;
    }
    m_connected = false;
    emit disconnected();
}

void Communication::startPolling()
{
    if (m_connected) {
        m_pollTimer->start();
        emit statusMessage("开始轮询数据");
    }
}

void Communication::stopPolling()
{
    m_pollTimer->stop();
}

void Communication::pollData()
{
    if (!m_modbusClient || !m_connected)
        return;

    if (m_modbusClient->state() != QModbusDevice::ConnectedState)
        return;

    QModbusDataUnit unit = readRequest();
    QModbusReply *reply = m_modbusClient->sendReadRequest(unit, 1);
    if (!reply) {
        emit statusMessage("读取请求发送失败");
        return;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit unit = reply->result();
            processReadResponse(unit);
            emit dataUpdated();
        } else {
            emit statusMessage(QString("读取错误: %1").arg(reply->errorString()));
        }
    });
}

QModbusDataUnit Communication::readRequest() const
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, 0, 8);
    return unit;
}

void Communication::processReadResponse(const QModbusDataUnit &unit)
{
    if (unit.valueCount() < 8)
        return;

    QMutexLocker locker(&m_mutex);
    m_columnOven1Temp = unit.value(0);
    m_flow1 = unit.value(1);
    m_flow2 = unit.value(2);
    m_pressure = static_cast<qint16>(unit.value(3));
    m_tcdVoltageA = static_cast<qint16>(unit.value(4));
    m_tcdVoltageB = static_cast<qint16>(unit.value(5));
    m_tcdVoltageAB = static_cast<qint16>(unit.value(6));
    m_tcdTemperature = static_cast<qint16>(unit.value(7));
}

void Communication::writeRegister(quint16 address, quint16 value, const QString &description)
{
    if (!m_modbusClient || !m_connected) {
        emit statusMessage("未连接，无法写入");
        return;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, 1);
    unit.setValue(0, value);

    emit logPacket("发送", QString("%1 [寄存器 0x%2 = %3]")
                               .arg(description)
                               .arg(address, 4, 16, QChar('0'))
                               .arg(value));

    QModbusReply *reply = m_modbusClient->sendWriteRequest(unit, 1);
    if (reply) {
        connect(reply, &QModbusReply::finished, this, [this, reply, description]() {
            if (reply->error() == QModbusDevice::NoError) {
                emit logPacket("接收", QString("%1 写入成功").arg(description));
            } else {
                emit logPacket("接收", QString("%1 写入失败: %2").arg(description, reply->errorString()));
            }
            reply->deleteLater();
        });
    } else {
        emit statusMessage("写入请求发送失败");
    }
}

void Communication::setTcdTemperature(quint16 value) { writeRegister(0x03E8, value, "设置TCD温度"); }
void Communication::setLampPowerA(quint16 value) { writeRegister(0x03E9, value, "设置灯丝功率A"); }
void Communication::setLampPowerB(quint16 value) { writeRegister(0x03EA, value, "设置灯丝功率B"); }
void Communication::enableDetector(bool enable) { writeRegister(0x03EB, enable ? 1 : 0, enable ? "开启检测器加热与灯丝供电" : "关闭检测器加热与灯丝供电"); }
void Communication::setChannelAVoltage(quint16 value) { writeRegister(0x03ED, value, "设置A通道电压"); }
void Communication::setChannelBVoltage(quint16 value) { writeRegister(0x03EE, value, "设置B通道电压"); }
void Communication::setChannelABVoltage(quint16 value) { writeRegister(0x03EF, value, "设置A-B通道电压"); }
void Communication::setPrecision(quint16 value) { writeRegister(0x03F1, value, "设置最小精度"); }
void Communication::setColumnOven1Temperature(quint16 value) { writeRegister(0x03FF, value, "设置柱温箱1温度"); }
void Communication::setColumnOven2Temperature(quint16 value) { writeRegister(0x03FF, value, "设置柱温箱2温度"); }
void Communication::setColumnOvenEnable(bool enable) { writeRegister(0x0400, enable ? 1 : 0, enable ? "开启柱温箱" : "关闭柱温箱"); }
void Communication::setSixWayValve1(bool on) { writeRegister(0x0404, on ? 1 : 0, on ? "六通阀1开启" : "六通阀1关闭"); }
void Communication::setSixWayValve2(bool on) { writeRegister(0x0404, on ? 1 : 0, on ? "六通阀2开启" : "六通阀2关闭"); }

void Communication::setValveBit(int valveIndex, bool on)
{
    if (valveIndex < 0 || valveIndex > 10) return;
    if (on)
        m_valveState |= (1 << valveIndex);
    else
        m_valveState &= ~(1 << valveIndex);

    writeRegister(0x0401, m_valveState, QString("设置电磁阀 NV%1 %2").arg(valveIndex+1).arg(on ? "开启" : "关闭"));
}

void Communication::setAllValvesOff()
{
    m_valveState = 0;
    writeRegister(0x0401, m_valveState, "关闭所有电磁阀");
}

void Communication::setFlow1Setpoint(quint16 value) { writeRegister(0x0402, value, "设置流量控制器1电压"); }
void Communication::setFlow2Setpoint(quint16 value) { writeRegister(0x0403, value, "设置流量控制器2电压"); }

void Communication::requestRegisterRead(quint16 address, std::function<void(quint16)> callback)
{
    if (!m_modbusClient || !m_connected) {
        callback(0);
        return;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, 1);
    QModbusReply *reply = m_modbusClient->sendReadRequest(unit, 1);
    if (!reply) {
        callback(0);
        return;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        quint16 value = 0;
        if (reply->error() == QModbusDevice::NoError) {
            const QModbusDataUnit result = reply->result();
            if (result.valueCount() > 0)
                value = result.value(0);
        }
        callback(value);
    });
}

void Communication::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        m_connected = true;
        emit connected();
        emit statusMessage("Modbus-TCP 已连接");
    } else if (state == QModbusDevice::UnconnectedState) {
        m_connected = false;
        emit disconnected();
        emit statusMessage("Modbus-TCP 已断开");
    }
}

void Communication::onErrorOccurred(QModbusDevice::Error error)
{
    emit statusMessage(QString("Modbus 错误: %1").arg(error));
}

bool Communication::isConnected() const { return m_connected; }

double Communication::columnOven1Temp() const { QMutexLocker locker(&m_mutex); return m_columnOven1Temp; }
double Communication::flow1() const { QMutexLocker locker(&m_mutex); return m_flow1; }
double Communication::flow2() const { QMutexLocker locker(&m_mutex); return m_flow2; }
double Communication::pressure() const { QMutexLocker locker(&m_mutex); return m_pressure; }
double Communication::tcdVoltageA() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageA; }
double Communication::tcdVoltageB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageB; }
double Communication::tcdVoltageAB() const { QMutexLocker locker(&m_mutex); return m_tcdVoltageAB; }
double Communication::tcdTemperature() const { QMutexLocker locker(&m_mutex); return m_tcdTemperature; }