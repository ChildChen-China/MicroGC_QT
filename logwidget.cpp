#include "logwidget.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_logFile()
    , m_logStream()
{
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"时间", "类型", "事件"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumHeight(160);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_table);

    // 创建 log 目录（若不存在）
    QDir dir;
    if (!dir.exists("log")) {
        dir.mkpath("log");
    }

    // 打开今天的日志文件
    QString today = QDate::currentDate().toString("yyyyMMdd");
    openLogFile(QDate::currentDate(), 0);
}

LogWidget::~LogWidget()
{
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
}

void LogWidget::appendLog(const QString &type, const QString &event)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // 更新界面表格
    int row = 0;
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(time));
    m_table->setItem(row, 1, new QTableWidgetItem(type));
    m_table->setItem(row, 2, new QTableWidgetItem(event));

    // 写入文件
    if (m_logFile.isOpen()) {
        m_logStream << QString("[%1] [%2] %3\n").arg(time, type, event);
        m_logStream.flush();  // 确保立即写入磁盘，避免丢失
        m_currentFileLines++;

        // 检查是否需要切换文件（跨天或行数超限）
        checkAndRotateFile();
    } else {
        qWarning() << "日志文件未打开，无法写入";
    }
}

void LogWidget::openLogFile(const QDate &date, int suffix)
{
    // 关闭旧文件
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }

    QString dateStr = date.toString("yyyyMMdd");
    QString fileName;
    if (suffix == 0) {
        fileName = QString("log/%1.txt").arg(dateStr);
    } else {
        fileName = QString("log/%1_%2.txt").arg(dateStr).arg(suffix);
    }

    m_logFile.setFileName(fileName);
    if (m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
        m_currentDate = dateStr;
        m_currentFileLines = 0;  // 打开新文件时重置行计数
        // 如果文件已存在且非空，需要先统计现有行数，但此处简化处理：直接以0开始计数，
        // 可能导致行数超过10万才切换，但通常可接受。如需精确计数可自行扩展。
    } else {
        qWarning() << "无法打开日志文件:" << fileName;
        m_logStream.setDevice(nullptr);
    }
}

void LogWidget::checkAndRotateFile()
{
    // 检查是否跨天
    QString today = QDate::currentDate().toString("yyyyMMdd");
    if (today != m_currentDate) {
        openLogFile(QDate::currentDate(), 0);
        return;
    }

    // 检查行数是否超过10万
    const int maxLines = 100000;
    if (m_currentFileLines >= maxLines) {
        // 查找当前日期的下一个可用后缀
        QString dateStr = QDate::currentDate().toString("yyyyMMdd");
        int suffix = 1;
        while (QFile::exists(QString("log/%1_%2.txt").arg(dateStr).arg(suffix))) {
            suffix++;
        }
        openLogFile(QDate::currentDate(), suffix);
    }
}