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

class QVBoxLayout;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QLabel;
class InteractivePlot;
class Communication;
class QPushButton;

//==========================================================
// SignalPlotPanel - 单个信号显示面板（含缩放、滤波切换等）
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
    bool m_rangeInitialized = false; // 是否已初始化坐标轴范围
};

//==========================================================
// MonitorTab - 主监视标签页
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

public slots:
    void startAutoSave();
    void startDataSave(const QString &fileName, int durationMinutes);
    void stopDataSave();
    void onDataUpdated();

    void setChannelAVisible(bool visible);
    void setChannelBVisible(bool visible);
    void setChannelABVisible(bool visible);
    void applyGlobalParameters();
    void setDetectorEnabled(bool enabled);

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
    void checkSettingFeedback();

private:
    void setupControlPanel(QVBoxLayout *layout);
    void setupSignalPanel(QVBoxLayout *layout);

    // ---- 数据缓存 ----
    QVector<double> m_time;
    QVector<double> m_rawA, m_rawB, m_rawAB;
    QVector<double> m_filtA, m_filtB, m_filtAB;
    int m_collectPoints;
    int m_averagePoints;

    // ---- 显示控制 ----
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

    // ---- UI 控件 ----
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

    // ---- 设置验证 ----
    QTimer *m_feedbackTimer;
    quint16 m_pendingTemp, m_pendingPowerA, m_pendingPowerB, m_pendingPrecision;
    quint16 m_pendingLevelA, m_pendingLevelB, m_pendingLevelAB;
    bool m_hasPendingTemp, m_hasPendingPowerA, m_hasPendingPowerB;
    bool m_hasPendingPrecision, m_hasPendingLevelA, m_hasPendingLevelB, m_hasPendingLevelAB;
    int m_retryTemp = 0;
    int m_retryPowerA = 0;
    int m_retryPowerB = 0;
    int m_retryPrecision = 0;
    int m_retryLevelA = 0;
    int m_retryLevelB = 0;
    int m_retryLevelAB = 0;

    // ---- 文件保存（主线程直接写） ----
    QFile m_saveFile;
    QTextStream m_saveStream;
    bool m_isSaving = false;
    qint64 m_startTime;          // 相对时间起点（毫秒）
    QTimer *m_autoStopTimer;     // 自动停止保存定时器（单次）


    // ---- 绘图刷新定时器 ----
    QTimer *m_plotTimer;
};

#endif // MONITORTAB_H