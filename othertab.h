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

    // 显示长度（暂未使用）
    int m_columnOvenLength = 1000;
    int m_pressureLength = 1000;
    int m_vacuumLength = 1000;
    int m_flow1Length = 1000;
    int m_flow2Length = 1000;
};

#endif // OTHERTAB_H