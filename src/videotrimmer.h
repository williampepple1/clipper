#pragma once

#include <QDialog>
#include <QString>

class TimelineSlider;
class QLabel;
class QLineEdit;
class QPushButton;
class QProcess;

class VideoTrimmer : public QDialog
{
    Q_OBJECT
public:
    explicit VideoTrimmer(const QString &videoPath, QWidget *parent = nullptr);
    ~VideoTrimmer() override;

    QString trimmedPath() const { return m_outputPath; }

signals:
    void trimCompleted(const QString &outputPath);
    void trimError(const QString &message);

private slots:
    void onRangeChanged(double start, double end);
    void onTrim();
    void onBrowseOutput();
    void onProcessFinished(int exitCode);

private:
    void loadVideoInfo();
    void updateTimeLabels();

    QString m_inputPath;
    QString m_outputPath;
    double m_durationSec = 0;

    TimelineSlider *m_timeline;
    QLabel *m_fileInfo;
    QLabel *m_startLabel;
    QLabel *m_endLabel;
    QLabel *m_durationLabel;
    QLabel *m_statusLabel;
    QLineEdit *m_outputEdit;
    QPushButton *m_trimBtn;
    QPushButton *m_browseBtn;
    QProcess *m_process = nullptr;
};
