#ifndef AUTOPROCESS_H
#define AUTOPROCESS_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <QStringList>
#include <QList>
#include <functional>

class Communication;

struct AutoProcessSettings
{
    int columnOvenTemp = 60;
    int tcdTemp = 120;
    int carrierFlow1Voltage = 5;
    int carrierFlow2Voltage = 5;
    double tempTolerance = 0.5;
    double startRecordMin = 0.01;
    double valveOpenMin = 0.1;
    double valveCloseMin = 0.8;
    double stopRecordMin = 2.0;
    double coolDownTemp = 50.0;
    int coolDownTimeoutSec = 1800;
};

class AutoProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit AutoProcessManager(Communication *comm, QObject *parent = nullptr);

    AutoProcessSettings settings() const;
    void setSettings(const AutoProcessSettings &s);
    QString currentStateName() const;

signals:
    void stateChanged(const QString &stateName);
    void logMessage(const QString &type, const QString &event);
    void deviceStateChanged(const QString &device, bool state);
    void saveDataTriggered();
    void stopDataSaveTriggered();
    void finished();

public slots:
    void start();
    void stop();
    void reset();

private slots:
    void onDataUpdated();
    void onProcessTimer();

private:
    enum State {
        Idle,
        SettingVerify,
        Heating,
        Stabilizing,
        Testing,
        CoolingDown,
        Shutdown
    };

    struct CommandToVerify {
        QString name;
        quint16 startAddr;
        QVector<quint16> expectedValues;
        std::function<void()> resend;
        int retryCount = 0;
    };

    void changeState(State newState);
    void sendInitialSettings();
    void sendTestSequenceCommand(double elapsedMin);
    void beginCooling();
    void enqueueVerify(const QString &name, quint16 address, quint16 expected,
                       std::function<void()> resend);
    void enqueueVerifyRange(const QString &name, quint16 startAddr,
                            const QVector<quint16> &expectedValues,
                            std::function<void()> resend);
    void processVerifyQueue();

    Communication *m_comm;
    AutoProcessSettings m_settings;
    State m_state;
    QTimer *m_processTimer;
    QDateTime m_testStartTime;
    QDateTime m_stabilizeStartTime;

    bool m_valveOpenDone = false;
    bool m_valveCloseDone = false;
    bool m_recordStarted = false;
    bool m_stopSaveSent = false;
    bool m_verifyReadPending = false;

    QList<CommandToVerify> m_verifyQueue;
};

#endif // AUTOPROCESS_H