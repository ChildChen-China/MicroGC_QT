#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QDate>

class QTableWidget;

class LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget *parent = nullptr);
    ~LogWidget();

public slots:
    void appendLog(const QString &type, const QString &event);

private:
    void openLogFile(const QDate &date, int suffix = 0);
    void checkAndRotateFile();

    QTableWidget *m_table;
    QFile m_logFile;
    QTextStream m_logStream;
    QString m_currentDate;
    int m_currentFileLines = 0;

    static const int MAX_DISPLAY_ROWS = 2000;   // 表格最多显示行数
};

#endif // LOGWIDGET_H