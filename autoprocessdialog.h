#ifndef AUTOPROCESSDIALOG_H
#define AUTOPROCESSDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QList>
#include "autoprocess.h"

class QTableWidget;

//==========================================================
// 时间轴总览（可视化）
//==========================================================
class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);
    void setGroups(const QList<AutoEventGroup> &groups);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<AutoEventGroup> m_groups;
};

//==========================================================
// 自动流程设置对话框
//==========================================================
class AutoProcessDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AutoProcessDialog(AutoProcessManager *mgr, QWidget *parent = nullptr);

private slots:
    void onAddCollect();
    void onAddValve();
    void onAddPolarity();
    void onRemoveSelected();
    void onSaveToFile();
    void onLoadFromFile();
    void onAccept();

private:
    void refreshTable();
    bool validate() const;

    AutoProcessManager *m_mgr;
    QTableWidget *m_table;
    TimelineWidget *m_timeline;

    bool m_refreshing = false;
};

#endif // AUTOPROCESSDIALOG_H