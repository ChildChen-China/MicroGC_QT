#include "logwidget.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
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
}

void LogWidget::appendLog(const QString &type, const QString &event)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    int row = 0;
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(time));
    m_table->setItem(row, 1, new QTableWidgetItem(type));
    m_table->setItem(row, 2, new QTableWidgetItem(event));
}