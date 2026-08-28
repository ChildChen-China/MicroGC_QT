#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>

class QTableWidget;

class LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);

public slots:
    void appendLog(const QString &type, const QString &event);

private:
    QTableWidget *m_table;
};

#endif // LOGWIDGET_H