#ifndef DATAPROCESSING_H
#define DATPROCESSING_H

#include <QDialog>
#include <QVector>
#include <QMap>
#include <QList>
#include <QString>
#include <QColor>

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
class QFormLayout;
class QGroupBox;
class QWidget;

//==========================================================
// 6 个通道
//==========================================================
enum ChannelIdx {
    CH_RAWA = 0,
    CH_FILTA,
    CH_RAWB,
    CH_FILTB,
    CH_RAWAB,
    CH_FILTAB,
    CH_COUNT
};

inline QString channelName(int idx)
{
    static const char *names[6] = {
        "A滤波前", "A滤波后",
        "B滤波前", "B滤波后",
        "A-B滤波前", "A-B滤波后"
    };
    if (idx < 0 || idx >= 6) return QString();
    return QString::fromUtf8(names[idx]);
}

//==========================================================
// 每个文件的数据
//==========================================================
struct FileEntry {
    QString filePath;
    QString displayName;
    QVector<double> time;
    QVector<double> channels[6];
    bool visibility[6];

    FileEntry() {
        visibility[CH_RAWA]   = false;
        visibility[CH_FILTA]  = true;
        visibility[CH_RAWB]   = false;
        visibility[CH_FILTB]  = true;
        visibility[CH_RAWAB]  = false;
        visibility[CH_FILTAB] = true;
    }

    void clear() {
        filePath.clear();
        displayName.clear();
        time.clear();
        for (int i = 0; i < 6; ++i) channels[i].clear();
    }
};

// 计算某个文件某通道的颜色（同类色）
QColor channelColor(int fileIdx, int channelIdx);

//==========================================================
// 积分设置对话框
//==========================================================
class IntegralSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IntegralSettingsDialog(QWidget *parent = nullptr);

    // 文件列表变化时由主对话框调用
    void setFiles(const QList<FileEntry> &files);

signals:
    void settingsChanged(QString integralType,
                         double waveWidth, double slopeThreshold,
                         double advance, double delay,
                         bool removeMean,
                         QString normalizeMode,
                         QList<QList<bool>> visibility);   // [fileIdx][channel]
private slots:
    void applySettings();
private:
    void rebuildChannelChecks();
    void loadSettingsToWidgets();
    void saveSettingsFromWidgets();

    QGroupBox *m_chkGroup;
    QFormLayout *m_chkFormLayout;
    QList<QList<QCheckBox*>> m_chkList;
    QStringList m_fileNames;
    QList<QList<bool>> m_fileVis;

    QComboBox *m_integralType;
    QDoubleSpinBox *m_waveWidth;
    QDoubleSpinBox *m_slopeThreshold;
    QDoubleSpinBox *m_advance;
    QDoubleSpinBox *m_delay;
    QCheckBox *m_chkRemoveMean;
    QComboBox *m_normalizeMode;
};

//==========================================================
// 积分工作类（同步，无多线程）
//==========================================================
class IntegrationWorker : public QObject
{
    Q_OBJECT
public:
    explicit IntegrationWorker(QObject *parent = nullptr);

    void setFiles(const QList<FileEntry> &files);        // 传入已完成去均值处理的数据
    void setParameters(double waveWidth, double slopeThreshold,
                       double advance, double delay,
                       const QString &integralType,
                       const QString &normalizeMode);

public slots:
    void processAutoIntegration();
signals:
    void integrationFinished(const QList<QStringList> &results);
    void warningsGenerated(const QList<QString> &warnings);
private:
    // 每个通道的积分结果
    struct PeakItem {
        QStringList row;    // 不含归一化列
        int fileIdx;
        int channelIdx;
        double area;
    };

    void integrateChannel(const QVector<double> &x, const QVector<double> &y,
                          const QString &fileLabel, const QString &channelLabel,
                          int fileIdx, int channelIdx,
                          QList<PeakItem> &peaks);
    double calculateArea(const QVector<double> &x, const QVector<double> &y,
                         int startIdx, int endIdx,
                         const QString &type);

    QList<FileEntry> m_files;
    double m_waveWidth;
    double m_slopeThreshold;
    double m_advance;
    double m_delay;
    QString m_integralType;
    QString m_normalizeMode;
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openFile();
    void clearAll();
    void showIntegralSettings();
    void toggleManualIntegral(bool active);
    void autoIntegrate();
    void handleRightClick(const QPointF &scenePos);
    void exportData();
    void resetView();
    void applyZoomMode(int id);
    void updateSettings(QString integralType,
                        double waveWidth, double slopeThreshold,
                        double advance, double delay,
                        bool removeMean,
                        QString normalizeMode,
                        QList<QList<bool>> visibility);
    void onIntegrationFinished(const QList<QStringList> &results);

private:
    void setupPlots();
    void rebuildGraphs();
    void updatePlotVisibility();
    void applyDataProcessing();
    void handlePlotHover(const QPoint &pos);
    bool parseFile(const QString &path, FileEntry &out);
    void addResultToTable(const QString &fileName, const QString &component,
                          double start, double end, const QString &type,
                          double retention, double area, double normalized);
    double calculateArea(const QVector<double> &x, const QVector<double> &y,
                         int startIdx, int endIdx, const QString &type);

    // 获取某文件某通道的处理后数据（按去均值开关）
    QVector<double> processedChannel(const FileEntry &f, int chIdx) const;

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

    QList<FileEntry> m_files;    // 原始数据

    QString m_integralType;
    double m_waveWidth;
    double m_slopeThreshold;
    double m_advance;
    double m_delay;
    bool m_removeMean;
    QString m_normalizeMode;

    IntegrationWorker *m_worker;
    QList<QString> m_manualWarnings;
};

#endif // DATAPROCESSING_H