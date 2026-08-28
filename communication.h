#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QTimer>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <functional>

class Communication : public QObject
{
    Q_OBJECT
public:
    explicit Communication(QObject *parent = nullptr);
    ~Communication();

    // 连接与断开
    void connectToDevice(const QString &ip, quint16 port);
    void disconnectDevice();

    // 启动/停止数据轮询
    void startPolling();
    void stopPolling();

    // 读取数据
    double columnOven1Temp() const;
    double flow1() const;
    double flow2() const;
    double pressure() const;
    double tcdVoltageA() const;
    double tcdVoltageB() const;
    double tcdVoltageAB() const;
    double tcdTemperature() const;

    // 命令写入
    void setTcdTemperature(quint16 value);
    void setLampPowerA(quint16 value);
    void setLampPowerB(quint16 value);
    void enableDetector(bool enable);
    void setChannelAVoltage(quint16 value);
    void setChannelBVoltage(quint16 value);
    void setChannelABVoltage(quint16 value);
    void setPrecision(quint16 value);
    void setColumnOven1Temperature(quint16 value);
    void setColumnOven2Temperature(quint16 value);
    void setColumnOvenEnable(bool enable);       // 新增：柱温箱开关
    void setSixWayValve1(bool on);
    void setSixWayValve2(bool on);
    void setValveBit(int valveIndex, bool on);
    void setAllValvesOff();
    void setFlow1Setpoint(quint16 value);
    void setFlow2Setpoint(quint16 value);

    // 通用读寄存器（异步回调）
    void requestRegisterRead(quint16 address, std::function<void(quint16)> callback);

    // 状态
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void dataUpdated();
    void statusMessage(const QString &message);
    void logPacket(const QString &direction, const QString &dataHex);

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void pollData();

private:
    void writeRegister(quint16 address, quint16 value, const QString &description);
    QModbusDataUnit readRequest() const;
    void processReadResponse(const QModbusDataUnit &unit);

    QModbusTcpClient *m_modbusClient;
    QTimer *m_pollTimer;
    bool m_connected;
    mutable QMutex m_mutex;

    double m_columnOven1Temp;
    double m_flow1;
    double m_flow2;
    double m_pressure;
    double m_tcdVoltageA;
    double m_tcdVoltageB;
    double m_tcdVoltageAB;
    double m_tcdTemperature;

    quint16 m_valveState;
};

#endif // COMMUNICATION_H