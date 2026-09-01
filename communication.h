#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <functional>

class Communication : public QObject
{
    Q_OBJECT
public:
    explicit Communication(QObject *parent = nullptr);
    ~Communication();

    void connectToDevice(const QString &ip, quint16 port);
    void disconnectDevice();

    void startPolling();
    void stopPolling();

    // 数据 getter
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
    void setColumnOvenEnable(bool enable);
    void setSixWayValve1(bool on);
    void setSixWayValve2(bool on);
    void setValveBit(int valveIndex, bool on);
    void setAllValvesOff();
    void setFlow1Setpoint(quint16 value);
    void setFlow2Setpoint(quint16 value);

    void requestRegisterRead(quint16 address, std::function<void(quint16)> callback);

    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void fastDataUpdated();      // 快速数据更新（TCD电压）
    void slowDataUpdated();      // 慢速数据更新（温度、流量、压力）
    void statusMessage(const QString &message);
    void logPacket(const QString &direction, const QString &dataHex);

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void pollFastData();
    void pollSlowData();

private:
    void writeRegister(quint16 address, quint16 value, const QString &description);
    QModbusDataUnit fastReadRequest() const;
    QModbusDataUnit slowReadRequest() const;
    void processFastResponse(const QModbusDataUnit &unit);
    void processSlowResponse(const QModbusDataUnit &unit);

    QModbusTcpClient *m_modbusClient;
    QTimer *m_fastTimer;
    QTimer *m_slowTimer;
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