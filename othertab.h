#ifndef OTHERTAB_H
#define OTHERTAB_H

#include <QWidget>
#include <QTimer>

class QPushButton;
class QHBoxLayout;
class QCheckBox;
class QButtonGroup;
class QGroupBox;
class InteractivePlot;
class QRadioButton;
class Communication;

class OtherTab : public QWidget
{
    Q_OBJECT

public:
    explicit OtherTab(QWidget *parent = nullptr);

    // 设置各通道显示长度（暂未使用，保留接口）
    void setColumnOvenLength(int points);
    void setPressureLength(int points);
    void setVacuumLength(int points);
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
    void scrollPlots();          // 平滑滚动槽函数

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void resetAllPlots();
    QHBoxLayout* setupFirstRow();
    QHBoxLayout* setupSecondRow();
    QHBoxLayout* setupThirdRow();

    QPushButton *m_resetBtn;
    QCheckBox *m_chkColumnOven;
    QCheckBox *m_chkPressure;
    QCheckBox *m_chkVacuum;
    QCheckBox *m_chkFlow1;
    QCheckBox *m_chkFlow2;
    QButtonGroup *m_zoomGroup;
    QCheckBox *m_crosshairCheck;

    InteractivePlot *m_plotColumnOven;
    InteractivePlot *m_plotPressure;
    InteractivePlot *m_plotVacuum;
    InteractivePlot *m_plotFlow1;
    InteractivePlot *m_plotFlow2;

    QGroupBox *m_groupColumnOven;
    QGroupBox *m_groupPressure;
    QGroupBox *m_groupVacuum;
    QGroupBox *m_groupFlow1;
    QGroupBox *m_groupFlow2;

    Communication *m_comm;

    bool m_autoScrollEnabled;
    QTimer *m_scrollTimer;          // 平滑滚动定时器
    qint64 m_startTime;             // 起始时间（毫秒）
};

#endif // OTHERTAB_H