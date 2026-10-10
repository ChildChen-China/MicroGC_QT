#ifndef MONITORTAB_H
#define MONITORTAB_H

#include <QWidget>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QGroupBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QList>
#include <QPair>

class QVBoxLayout;
class QLineEdit;
class QLabel;
class InteractivePlot;
class Communication;

//==========================================================
// SignalPlotPanel
//==========================================================
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
    bool m_rangeInitialized = false;
};

//==========================================================
// MonitorTab
//==========================================================
class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(QWidget *parent = nullptr);
    ~MonitorTab();

    void setCommunication(Communication *comm);
    void setCollectPoints(int points);
    void setAveragePoints(int points);

    int getParameter(const QString &key) const;
    void setParameter(const QString &key, int value);

    void setTcdPowered(bool powered);

public slots:
    // 由自动流程触发的数据采集（从当前时刻开始，不写历史缓冲）
    void startDataSaveNow();
    void stopDataSave();

    // 由自动流程触发的极性反转
    void startPolarityReversal();
    void stopPolarityReversal();

    void onDataUpdated();

    void setChannelAVisible(bool visible);
    void setChannelBVisible(bool visible);
    void setChannelABVisible(bool visible);
    void applyGlobalParameters();

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
    void setZS();
    void setPrecision();
    void setRF();
    void choosePath();
    void stopFileSave();
    void updatePlots(bool force = false);
    void toggleFilterA();
    void toggleFilterB();
    void toggleFilterAB();
    void updateDisplayLength();
    void checkSettingFeedback();

private:
    void setupControlPanel(QVBoxLayout *layout);
    void setupSignalPanel(QVBoxLayout *layout);

    // 数据缓存
    QVector<double> m_time;
    QVector<double> m_rawA, m_rawB, m_rawAB;     // 原始值
    QVector<double> m_filtA, m_filtB, m_filtAB;  // 滤波值
    QVector<double> m_dispA, m_dispB, m_dispAB;  // 显示值（已应用极性反转）
    int m_collectPoints;
    int m_averagePoints;

    // 显示控制
    bool m_showFiltA;
    bool m_showFiltB;
    bool m_showFiltAB;
    SignalPlotPanel *m_panelA;
    SignalPlotPanel *m_panelB;
    SignalPlotPanel *m_panelAB;
    QPushButton *m_filterBtnA;
    QPushButton *m_filterBtnB;
    QPushButton *m_filterBtnAB;

    Communication *m_comm;

    // UI 控件
    QSpinBox *m_tempEdit;
    QSpinBox *m_powerAEdit;
    QSpinBox *m_powerBEdit;
    QSpinBox *m_levelAEdit;
    QSpinBox *m_levelBEdit;
    QSpinBox *m_levelABEdit;
    QSpinBox *m_zsEdit;
    QComboBox *m_precisionEdit;
    QPushButton *m_rfBtn;
    QLineEdit *m_fileNameEdit;
    QSpinBox *m_durationSpin;
    QLineEdit *m_pathEdit;

    // 设置验证
    QTimer *m_feedbackTimer;
    quint16 m_pendingTemp, m_pendingPowerA, m_pendingPowerB, m_pendingPrecision;
    quint16 m_pendingLevelA, m_pendingLevelB, m_pendingLevelAB, m_pendingZS;
    bool m_hasPendingTemp, m_hasPendingPowerA, m_hasPendingPowerB;
    bool m_hasPendingPrecision, m_hasPendingLevelA, m_hasPendingLevelB, m_hasPendingLevelAB, m_hasPendingZS;
    int m_retryTemp = 0;
    int m_retryPowerA = 0;
    int m_retryPowerB = 0;
    int m_retryPrecision = 0;
    int m_retryLevelA = 0;
    int m_retryLevelB = 0;
    int m_retryLevelAB = 0;

    // 文件保存
    QFile m_saveFile;
    QTextStream m_saveStream;
    bool m_isSaving = false;
    qint64 m_startTime;
    QTimer *m_autoStopTimer;

    // 绘图刷新
    QTimer *m_plotTimer;

    // TCD 电源状态与时间轴累积
    bool m_tcdPowered = false;
    qint64 m_tcdActiveMs = 0;
    qint64 m_tcdLastTick = 0;

    // 极性反转
    bool m_polarityActive = false;   // 当前是否正在反转
    // 显示值用 m_dispA/B/AB 记录，反转折点之后新数据取负；
    // 已经记录的点不变，符合"反转之前的不能也反转"
};

#endif // MONITORTAB_H