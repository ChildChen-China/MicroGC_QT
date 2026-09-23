#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QList>
#include <QVector>
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
    void setTcdPowered(bool powered);
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
    double tcdPowerFA() const;
    double tcdPowerFB() const;
    double tcdMeasureTemp() const;

    // 命令写入
    void setTcdTemperature(quint16 value);
    void setLampPowerA(quint16 value);
    void setLampPowerB(quint16 value);
    void setChannelAVoltage(quint16 value);
    void setChannelBVoltage(quint16 value);
    void setChannelABVoltage(quint16 value);
    void setZS(quint16 value);
    void setPrecision(quint16 value);
    void setRF(quint16 value);
    void setColumnOven1Temperature(quint16 value);
    void setColumnOven2Temperature(quint16 value);
    void setColumnOvenEnable(bool enable);
    void setSixWayValve1(bool on);
    void setSixWayValve2(bool on);
    void setValveBit(int valveIndex, bool on);
    void setAllValvesOff();
    void setFlow1Voltage(quint16 value);
    void setFlow2Voltage(quint16 value);

    quint16 diagRegister() const;
    quint16 m_diagRegister = 0;

    // 批量写
    void writeMultipleRegisters(quint16 startAddr, const QVector<quint16> &values, const QString &description);

    // 读
    void requestRegisterRead(quint16 address, std::function<void(quint16)> callback);
    void requestRegisterReadRange(quint16 startAddr, quint16 count, std::function<void(const QVector<quint16>&)> callback);

    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void fastDataUpdated();
    void slowDataUpdated();
    void statusMessage(const QString &message);
    void logPacket(const QString &direction, const QString &dataHex);
    void faultBitsChanged(quint16 diag);

private slots:
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void onFastTimer();
    void onSlowTimer();


private:
    struct Request {
        enum Type { Write, WriteMultiple, Read, ReadRange, FastPoll, SlowPoll };
        Type type = FastPoll;
        quint16 address = 0;
        quint16 value = 0;
        quint16 count = 1;
        QVector<quint16> values;
        QString description;
        std::function<void(quint16)> readCallback;
        std::function<void(const QVector<quint16>&)> readRangeCallback;
    };

    void enqueue(const Request &req);
    void processQueue();
    void sendRequest(const Request &req);
    void writeRegister(quint16 address, quint16 value, const QString &description);

    void processFastResponse(const QModbusDataUnit &unit);
    void processSlowResponse(const QModbusDataUnit &unit, quint16 startAddr);

    int priorityOf(Request::Type t) const;

    QModbusTcpClient *m_client = nullptr;
    QTimer *m_fastTimer = nullptr;
    QTimer *m_slowTimer = nullptr;
    bool m_connected = false;
    bool m_busy = false;
    bool m_tcdPowered = false;
    qint64 m_lastFastPollTime = 0;

    QList<Request> m_queue;
    Request m_current;
    mutable QMutex m_mutex;

    double m_columnOven1Temp = 0;
    double m_flow1 = 0;
    double m_flow2 = 0;
    double m_pressure = 0;
    double m_tcdVoltageA = 0;
    double m_tcdVoltageB = 0;
    double m_tcdVoltageAB = 0;
    double m_tcdPowerFA = 0;
    double m_tcdPowerFB = 0;
    double m_tcdMeasureTemp = 0;

    quint16 m_valveState = 0;
};

#endif // COMMUNICATION_H