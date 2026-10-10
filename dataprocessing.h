#ifndef DATAPROCESSING_H
#define DATAPROCESSING_H

#include <QDialog>
#include <QVector>
#include <QMap>
#include <QList>
#include <QString>

class InteractivePlot;
class QButtonGroup;
class QTableWidget;
class QPushButton;
class QLabel;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCPItemStraightLine;

//==========================================================
// 积分设置对话框（非模态）
//==========================================================
class IntegralSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IntegralSettingsDialog(QWidget *parent = nullptr);
signals:
    // [修改点] 新增 bool removeMean 参数
    void settingsChanged(QMap<QString, bool> visibility,
                         QString integralType,
                         double waveWidth,
                         double slopeThreshold,
                         double advance,
                         double delay,
                         bool removeMean);
private slots:
    void applySettings();
private:
    QCheckBox *m_chkRawA;
    QCheckBox *m_chkFiltA;
    QCheckBox *m_chkRawB;
    QCheckBox *m_chkFiltB;
    QCheckBox *m_chkRawAB;
    QCheckBox *m_chkFiltAB;
    QCheckBox *m_chkRemoveMean; // [修改点] 去均值复选框
    QComboBox *m_integralType;
    QDoubleSpinBox *m_waveWidth;
    QDoubleSpinBox *m_slopeThreshold;
    QDoubleSpinBox *m_advance;
    QDoubleSpinBox *m_delay;
};

//==========================================================
// 积分工作线程类（改为普通类，取消多线程）
//==========================================================
class IntegrationWorker : public QObject
{
    Q_OBJECT
public:
    explicit IntegrationWorker(QObject *parent = nullptr);
    void setData(const QVector<double> &time,
                 const QVector<double> &rawA,
                 const QVector<double> &filtA,
                 const QVector<double> &rawB,
                 const QVector<double> &filtB,
                 const QVector<double> &rawAB,
                 const QVector<double> &filtAB);
    void setParameters(double waveWidth, double slopeThreshold,
                       double advance, double delay,
                       const QString &integralType,
                       QMap<QString, bool> visibility);
public slots:
    void processAutoIntegration();
signals:
    void integrationFinished(const QList<QStringList> &results);
    void warningsGenerated(const QList<QString> &warnings);
private:
    void integrateChannel(const QVector<double> &x, const QVector<double> &y,
                          const QString &channelName, const QString &type,
                          QList<QStringList> &results);
    double calculateArea(const QVector<double> &x, const QVector<double> &y,
                         int startIdx, int endIdx,
                         const QString &type);
    QVector<double> m_time;
    QVector<double> m_rawA, m_filtA, m_rawB, m_filtB, m_rawAB, m_filtAB;
    double m_waveWidth;
    double m_slopeThreshold;
    double m_advance;
    double m_delay;
    QString m_integralType;
    QMap<QString, bool> m_visibility;
    QList<QString> m_warnings;
};

//==========================================================
// 数据处理对话框
//==========================================================
class DataProcessingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DataProcessingDialog(QWidget *parent = nullptr);
    ~DataProcessingDialog();
private slots:
    void openFile();
    void showIntegralSettings();
    void toggleManualIntegral(bool active);
    void autoIntegrate();
    void handleRightClick(const QPointF &scenePos);
    void exportData();
    void resetView();
    void applyZoomMode(int id);
    // [修改点] 新增 bool removeMean 参数
    void updateSettings(QMap<QString, bool> visibility,
                        QString integralType,
                        double waveWidth,
                        double slopeThreshold,
                        double advance,
                        double delay,
                        bool removeMean);
    void onIntegrationFinished(const QList<QStringList> &results);
private:
    void setupPlots();
    void updatePlotVisibility();
    void applyDataProcessing(); // [修改点] 新增：根据去均值开关刷新数据
    void addResultToTable(const QString &fileName, const QString &component,
                          double start, double end, const QString &type,
                          double retention, double area);
    double calculateArea(const QVector<double> &x, const QVector<double> &y,
                         int startIdx, int endIdx, const QString &type);

    InteractivePlot *m_plot;
    QButtonGroup *m_zoomGroup;
    QTableWidget *m_resultTable;
    QPushButton *m_manualIntegralBtn;
    QLabel *m_statusLabel;
    IntegralSettingsDialog *m_settingsDialog;
    bool m_manualIntegralMode;
    bool m_selectingStart;
    double m_startPoint;
    QCPItemStraightLine *m_startLine;
    QCPItemStraightLine *m_endLine;

    // 原始数据（绝对不修改）
    QVector<double> m_timeOrig;
    QVector<double> m_rawAOrig, m_filtAOrig, m_rawBOrig, m_filtBOrig, m_rawABOrig, m_filtABOrig;

    // 当前用于绘图和积分的数据（可能是原始数据，也可能是去均值后的数据）
    QVector<double> m_time;
    QVector<double> m_rawA, m_filtA, m_rawB, m_filtB, m_rawAB, m_filtAB;

    QMap<QString, bool> m_visibility;
    QString m_integralType;
    double m_waveWidth;
    double m_slopeThreshold;
    double m_advance;
    double m_delay;
    bool m_removeMean; // [修改点] 去均值标志

    IntegrationWorker *m_worker; // [修改点] 取消 QThread
    QList<QString> m_manualWarnings;
};
#endif // DATAPROCESSING_H