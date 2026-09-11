#ifndef CONTROLTAB_H
#define CONTROLTAB_H

#include <QWidget>
#include <QGraphicsView>
#include <QDialog>
#include "interactiveplot.h"
#include "gassceneitems.h"

class QGraphicsScene;
class QFormLayout;
class QSpinBox;
class QPushButton;
class QLabel;
class QTimer;
class Communication;

//==========================================================
// RealtimeDetailDialog 基类
//==========================================================
class RealtimeDetailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RealtimeDetailDialog(const QString &title, QWidget *parent = nullptr);
    void setCommunication(Communication *comm);

signals:
    void lengthSet(int points);

protected:
    QFormLayout *m_infoLayout;
    InteractivePlot *m_plot;
    QSpinBox *m_lengthSpin;
    QPushButton *m_clearBtn;
    QPushButton *m_restoreBtn;
    QPushButton *m_setLengthBtn;

    QTimer *m_updateTimer;
    int m_dialogType;               // 0=流量1,1=流量2,2=压力,3=TCD,4=柱温箱
    Communication *m_comm;
    bool m_autoScrollEnabled;

    void addInfoRow(const QString &label, QWidget *widget);
    virtual void updateFromComm();
    virtual void updateValueLabel(double value) = 0;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void clear();
    void restoreView();
};

//==========================================================
// 流量控制器对话框（设置电压，显示流量值）
//==========================================================
class FlowControllerDialog : public RealtimeDetailDialog
{
    Q_OBJECT
public:
    explicit FlowControllerDialog(const QString &title, QWidget *parent = nullptr);

signals:
    void flowVoltageSet(double value);

protected:
    void updateValueLabel(double value) override;

private:
    QPushButton *m_setFlowBtn;
    QLabel *m_currentLabel;
};

//==========================================================
// 压力传感器对话框
//==========================================================
class PressureSensorDialog : public RealtimeDetailDialog
{
    Q_OBJECT
public:
    explicit PressureSensorDialog(const QString &title, QWidget *parent = nullptr);

protected:
    void updateValueLabel(double value) override;

private:
    QLabel *m_currentLabel;
};

//==========================================================
// TCD 对话框
//==========================================================
class TCDDialog : public RealtimeDetailDialog
{
    Q_OBJECT
public:
    explicit TCDDialog(const QString &title, QWidget *parent = nullptr);

protected:
    void updateValueLabel(double value) override;

private:
    QLabel *m_currentLabel;
};

//==========================================================
// 柱温箱对话框
//==========================================================
class ColumnOvenDialog : public RealtimeDetailDialog
{
    Q_OBJECT
public:
    explicit ColumnOvenDialog(const QString &title, QWidget *parent = nullptr);

signals:
    void tempSetpointSet(double value);

protected:
    void updateValueLabel(double value) override;

private:
    QPushButton *m_setTempBtn;
    QLabel *m_currentLabel;
};

//==========================================================
// ControlTab
//==========================================================
class ControlTab : public QWidget
{
    Q_OBJECT

public:
    explicit ControlTab(QWidget *parent = nullptr);
    void setCommunication(Communication *comm);

signals:
    void commandRequested(const QString &device, bool state);
    void logMessage(const QString &type, const QString &event);
    void sixWayValveToggled();
    void displayLengthChanged(const QString &device, int length);

public slots:
    void updateDeviceState(const QString &device, bool state);
    void showRealtimeDialog(const QString &title);
    void handleValveClicked(ValveItem *valve);
    void handleSixWayClicked(SixWayValveItem *valve);
    void updateRealtimeLabels();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    Communication *m_comm;
};

#endif // CONTROLTAB_H