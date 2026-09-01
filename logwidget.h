#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

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
    QString m_currentDate;      // 当前文件对应的日期 (yyyyMMdd)
    int m_currentFileLines = 0; // 当前文件已写入的行数
};

#endif // LOGWIDGET_H