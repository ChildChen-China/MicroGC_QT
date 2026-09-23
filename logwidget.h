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
    // 只上界面，不写文件（用于普通状态、流程日志）
    void appendLog(const QString &type, const QString &event);

    // 只写文件，不上界面（用于报文、收发等详细日志）
    void appendFileOnly(const QString &type, const QString &event);

private:
    void openLogFile(const QDate &date);
    void checkAndRotateFile();
    int countLinesInFile(const QString &fileName);

    QTableWidget *m_table;
    QFile m_logFile;
    QTextStream m_logStream;
    QString m_currentDate;          // 当前文件的日期（yyyyMMdd）
    int m_currentFileSuffix = 1;    // 当前文件编号后缀（从 1 开始）
    int m_currentFileLines = 0;     // 当前文件已写入行数

    static const int MAX_DISPLAY_ROWS = 2000;    // 界面表格最多显示行数
    static const int MAX_FILE_LINES   = 100000;  // 单个文件最多行数
};

#endif // LOGWIDGET_H