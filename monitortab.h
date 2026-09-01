#ifndef MONITORTAB_H
#define MONITORTAB_H

#include <QWidget>
#include <QVector>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QGroupBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>

class QVBoxLayout;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QLabel;
class InteractivePlot;
class Communication;
class QPushButton;

// 文件保存工作类（在独立线程中运行）
class SaveWorker : public QObject
{
    Q_OBJECT
public:
    explicit SaveWorker(QObject *parent = nullptr);
    ~SaveWorker();

public slots:
    void startSaving(const QString &fileName, int durationMinutes);
    void stopSaving();
    void appendData(const QString &dataLine);

private:
    QFile m_file;
    QTextStream m_stream;
    QTimer *m_timer;
};

// 滤波工作类（在独立线程中运行）
class FilterWorker : public QObject
{
    Q_OBJECT
public:
    explicit FilterWorker(QObject *parent = nullptr);

public slots:
    void processFilter(const QVector<double> &time,
                       const QVector<double> &rawA,
                       const QVector<double> &rawB,
                       const QVector<double> &rawAB,
                       int averagePoints);

signals:
    void filterCompleted(const QVector<double> &filtA,
                         const QVector<double> &filtB,
                         const QVector<double> &filtAB);
};

// 信号显示面板类（声明）
class SignalPlotPanel : public QGroupBox
{
    Q_OBJECT
public:
    explicit SignalPlotPanel(const QString &title, QWidget *parent = nullptr);

    QPushButton* filterButton() const;
    bool isPaused() const;
    void setData(const QVector<double> &x, const QVector<double> &y);

    void setSnapshot(const QVector<double>& x, const QVector<double>& rawY, const QVector<double>& filtY);
    void clearSnapshot();
    bool hasSnapshot() const;
    QVector<double> snapshotX() const;
    QVector<double> snapshotRawY() const;
    QVector<double> snapshotFiltY() const;

signals:
    void pauseStateChanged(bool paused);

private:
    void applyInversion();

    InteractivePlot *m_plot;
    QPushButton *m_pauseBtn;
    QPushButton *m_invertBtn;
    QPushButton *m_resetBtn;
    QPushButton *m_crosshairBtn;
    QPushButton *m_filterBtn;
    QButtonGroup *m_zoomGroup;
    QVector<double> m_x, m_y;
    QVector<double> m_snapshotX, m_snapshotRawY, m_snapshotFiltY;
    bool m_inverted;
    bool m_hasSnapshot;
    bool m_xRangeInitialized = false;
    bool m_rangeInitialized = false;
};

class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(QWidget *parent = nullptr);
    ~MonitorTab();

    void setCommunication(Communication *comm);
    void setCollectPoints(int points);
    void setAveragePoints(int points);

    int getParameter(const QString &key) const;                 // 读取参数值
    void setParameter(const QString &key, int value);           // 设置参数值（不发送命令）

public slots:
    void startAutoSave();
    void startDataSave(const QString &fileName, int durationMinutes);
    void stopDataSave();
    void onDataUpdated();

    void setChannelAVisible(bool visible);
    void setChannelBVisible(bool visible);
    void setChannelABVisible(bool visible);
    void applyGlobalParameters();   // 发送所有设置命令
    void setDetectorEnabled(bool enabled);
    void onFastDataUpdated();
    void onSlowDataUpdated();
signals:
    void logMessage(const QString &type, const QString &event);

private slots:
    void refreshParameters();
    void setTemperature();
    void setPowerA();
    void setPowerB();
    void setLevelA();
    void setLevelB();
    void setLevelAB();
    void setPrecision();
    void choosePath();
    void stopFileSave();
    void updatePlots(bool force = false);
    void toggleFilterA();
    void toggleFilterB();
    void toggleFilterAB();
    void updateDisplayLength();
    void onFilterCompleted(const QVector<double> &filtA,
                           const QVector<double> &filtB,
                           const QVector<double> &filtAB);
    void checkSettingFeedback();

private:
    void setupControlPanel(QVBoxLayout *layout);
    void setupSignalPanel(QVBoxLayout *layout);


    QVector<double> m_time;
    QVector<double> m_rawA, m_rawB, m_rawAB;
    QVector<double> m_filtA, m_filtB, m_filtAB;
    int m_collectPoints;
    int m_averagePoints;

    bool m_showFiltA;
    bool m_showFiltB;
    bool m_showFiltAB;

    SignalPlotPanel *m_panelA;
    SignalPlotPanel *m_panelB;
    SignalPlotPanel *m_panelAB;

    QPushButton *m_filterBtnA;
    QPushButton *m_filterBtnB;
    QPushButton *m_filterBtnAB;

    QThread *m_saveThread;
    SaveWorker *m_saveWorker;

    QThread *m_filterThread;
    FilterWorker *m_filterWorker;

    Communication *m_comm;

    QPushButton  *m_enableCheck;
    QLabel *m_tempValueLabel;
    QSpinBox *m_tempEdit;
    QSpinBox *m_powerAEdit;
    QSpinBox *m_powerBEdit;
    QSpinBox *m_levelAEdit;
    QSpinBox *m_levelBEdit;
    QSpinBox *m_levelABEdit;
    QComboBox *m_precisionEdit;
    QLineEdit *m_fileNameEdit;
    QSpinBox  *m_durationSpin;
    QLineEdit *m_pathEdit;

    QTimer *m_feedbackTimer;
    quint16 m_pendingTemp, m_pendingPowerA, m_pendingPowerB, m_pendingPrecision;
    quint16 m_pendingLevelA, m_pendingLevelB, m_pendingLevelAB;
    bool m_hasPendingTemp, m_hasPendingPowerA, m_hasPendingPowerB;
    bool m_hasPendingPrecision, m_hasPendingLevelA, m_hasPendingLevelB, m_hasPendingLevelAB;

    int m_lastSavedIndex;

    int m_retryTemp = 0;
    int m_retryPowerA = 0;
    int m_retryPowerB = 0;
    int m_retryPrecision = 0;
    int m_retryLevelA = 0;
    int m_retryLevelB = 0;
    int m_retryLevelAB = 0;
    QTimer *m_plotTimer;
    bool m_isSaving = false;
    qint64 m_startTime;
    // 用于同步写入的变量
    bool m_waitingForFilter = false;
    int m_filtDataLength = 0;   // 当前滤波数据对应的原始数据长度（索引上限）
    QFile m_saveFile;          // 主线程直接写文件
    QTextStream m_saveStream;  // 文本流

};

#endif // MONITORTAB_H