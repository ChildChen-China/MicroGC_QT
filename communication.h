#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QQueue>
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

    // 数据 getter（线程安全）
    double columnOven1Temp() const;
    double flow1() const;
    double flow2() const;
    double pressure() const;
    double tcdVoltageA() const;
    double tcdVoltageB() const;
    double tcdVoltageAB() const;
    double tcdTemperature() const;

    // 命令写入（使用慢速队列）
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

    // 按需读取（使用慢速队列）
    void requestRegisterRead(quint16 address, std::function<void(quint16)> callback);

    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void fastDataUpdated();
    void slowDataUpdated();
    void statusMessage(const QString &message);
    void logPacket(const QString &direction, const QString &dataHex);

private slots:
    void onFastStateChanged(QModbusDevice::State state);
    void onSlowStateChanged(QModbusDevice::State state);
    void onFastErrorOccurred(QModbusDevice::Error error);
    void onSlowErrorOccurred(QModbusDevice::Error error);
    void pollFastData();
    void pollSlowData();

private:
    // 慢速请求类型定义
    struct SlowRequest {
        enum Type { Read, Write };
        Type type;
        quint16 address;
        quint16 value;
        QString description;
        std::function<void(quint16)> readCallback;  // 仅读取时有效
    };

    void writeRegister(QModbusTcpClient *client, quint16 address, quint16 value, const QString &description);
    void enqueueSlowRead(quint16 address, std::function<void(quint16)> callback);
    void enqueueSlowWrite(quint16 address, quint16 value, const QString &description);
    void processSlowQueue();

    QModbusDataUnit fastReadRequest() const;
    QModbusDataUnit slowReadRequest() const;
    void processFastResponse(const QModbusDataUnit &unit);
    void processSlowResponse(const QModbusDataUnit &unit);

    // 双客户端
    QModbusTcpClient *m_fastClient;   // 快速轮询（TCD电压）
    QModbusTcpClient *m_slowClient;   // 慢速轮询、写入、按需读取

    QTimer *m_fastTimer;
    QTimer *m_slowTimer;

    bool m_fastConnected;
    bool m_slowConnected;
    mutable QMutex m_mutex;

    // 慢速请求队列
    QQueue<SlowRequest> m_slowQueue;
    bool m_slowBusy;

    // 数据缓存
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