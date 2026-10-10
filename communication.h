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
    void setTcdPowered(bool powered);   // 仅用于指示灯，不影响轮询
    void startPolling();
    void stopPolling();

    // 数据 getter
    double tc4Temp() const;             // 0x0000 TC4当前温度（°C）
    double flow1() const;               // 0x0001 流量控制器1流量值（0~100）
    double flow2() const;               // 0x0002 流量控制器2流量值（0~100）
    double pressure() const;            // 0x0003 压力（0~5000，0.1 kPa/计数）
    double tcdVoltageA() const;         // 0x000B/C 合成 s32，单位 uV
    double tcdVoltageB() const;         // 0x000D/E
    double tcdVoltageAB() const;        // 0x000F/10
    double tcdPowerFA() const;          // 0x0007 0~100
    double tcdPowerFB() const;          // 0x0008 0~100
    double tcdMeasureTemp() const;      // 0x0009 0.01°C/计数 → 返回 °C
    double columnOven1Temp() const;

    quint16 diagRegister() const;       // 0x000A 故障状态字
    quint16 m_diagRegister = 0;

    // 命令写入
    void setTcdTemperature(quint16 value);       // 0x03E8 0.01°C
    void setLampPowerA(quint16 value);           // 0x03E9 0~100
    void setLampPowerB(quint16 value);           // 0x03EA 0~100
    void setChannelAVoltage(quint16 value);      // 0x03ED -1200~+1200 (s16 位模式)
    void setChannelBVoltage(quint16 value);      // 0x03EE
    void setChannelABVoltage(quint16 value);     // 0x03EF
    void setZS(quint16 value);                   // 0x03F0 1~4
    void setPrecision(quint16 value);            // 0x03F1 1~1000
    void setRF(quint16 value);                   // 0x03F2 仅写1
    void setColumnOven1Temperature(quint16 value);// 0x03FF TC4目标温度
    void setColumnOven2Temperature(quint16 value);// 保留（同1）
    void setColumnOvenEnable(bool enable);       // 0x0400 TC4加热开关
    void setSixWayValve1(bool on);               // 0x0404
    void setSixWayValve2(bool on);               // 0x0404（值2）
    void setValveBit(int valveIndex, bool on);   // 0x0401 主电磁阀
    void setAllValvesOff();
    void setFlow1Voltage(quint16 value);         // 0x0402 0~5000 mV
    void setFlow2Voltage(quint16 value);         // 0x0403 0~5000 mV

    // 批量写
    void writeMultipleRegisters(quint16 startAddr, const QVector<quint16> &values, const QString &description);

    // 读
    void requestRegisterRead(quint16 address, std::function<void(quint16)> callback);
    void requestRegisterReadRange(quint16 startAddr, quint16 count,
                                  std::function<void(const QVector<quint16>&)> callback);

    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void fastDataUpdated();
    void slowDataUpdated();
    void statusMessage(const QString &message);
    void logPacket(const QString &direction, const QString &dataHex);

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
    bool m_tcdPowered = false;          // 仅指示灯使用
    qint64 m_lastFastPollTime = 0;

    QList<Request> m_queue;
    Request m_current;
    mutable QMutex m_mutex;

    // 数据缓存
    double m_tc4Temp = 0;               // 0x0000
    double m_flow1 = 0;                 // 0x0001
    double m_flow2 = 0;                 // 0x0002
    double m_pressure = 0;              // 0x0003
    double m_tcdPowerFA = 0;            // 0x0007
    double m_tcdPowerFB = 0;            // 0x0008
    double m_tcdMeasureTemp = 0;        // 0x0009 (原始 0.01°C 计数)
    double m_tcdVoltageA = 0;           // 0x000B/C s32 uV
    double m_tcdVoltageB = 0;           // 0x000D/E
    double m_tcdVoltageAB = 0;          // 0x000F/10

    quint16 m_valveState = 0;
};

#endif // COMMUNICATION_H