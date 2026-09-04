#ifndef OTHERTAB_H
#define OTHERTAB_H

#include <QWidget>
#include <QTimer>

class QPushButton;
class QCheckBox;
class QButtonGroup;
class QGroupBox;
class QRadioButton;
class QHBoxLayout;
class QVBoxLayout;
class InteractivePlot;
class Communication;

class OtherTab : public QWidget
{
    Q_OBJECT

public:
    explicit OtherTab(QWidget *parent = nullptr);

    // 设置各通道显示长度
    void setColumnOvenLength(int points);
    void setPressureLength(int points);
    void setTcdTempLength(int points);
    void setFlow1Length(int points);
    void setFlow2Length(int points);

    void setCommunication(Communication *comm);

signals:
    void logMessage(const QString &type, const QString &event);

private slots:
    void applyZoomMode();
    void setCrosshairEnabled(bool enabled);
    void toggleSensorVisible();
    void updateFromComm();
    void scrollPlots();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QHBoxLayout* setupFirstRow();          // 顶部选项行
    QVBoxLayout* setupPlotLayout();        // 图表区域（包含上下两个水平布局）

    void resetAllPlots();

    QPushButton *m_resetBtn;
    QCheckBox *m_chkColumnOven;
    QCheckBox *m_chkPressure;
    QCheckBox *m_chkTcdTemp;
    QCheckBox *m_chkFlow1;
    QCheckBox *m_chkFlow2;
    QButtonGroup *m_zoomGroup;
    QCheckBox *m_crosshairCheck;

    InteractivePlot *m_plotColumnOven;
    InteractivePlot *m_plotPressure;
    InteractivePlot *m_plotTcdTemp;
    InteractivePlot *m_plotFlow1;
    InteractivePlot *m_plotFlow2;

    QGroupBox *m_groupColumnOven;
    QGroupBox *m_groupPressure;
    QGroupBox *m_groupTcdTemp;
    QGroupBox *m_groupFlow1;
    QGroupBox *m_groupFlow2;

    Communication *m_comm;

    bool m_autoScrollEnabled;

    // 显示长度
    int m_columnOvenLength = 1000;
    int m_pressureLength = 1000;
    int m_tcdTempLength = 1000;
    int m_flow1Length = 1000;
    int m_flow2Length = 1000;

    QTimer *m_scrollTimer;
    qint64 m_startTime;

    // 两行图表布局
    QHBoxLayout *m_topRowLayout;      // 第一行（柱温箱、压力、TCD温度）
    QHBoxLayout *m_bottomRowLayout;   // 第二行（流量器1、流量器2）
    QVBoxLayout *m_plotLayout;        // 垂直布局，包含上面两个行布局
};

#endif // OTHERTAB_H