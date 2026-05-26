#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QRect>
#include <memory>

#include "platformpresets.h"

class Recorder;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onPresetChanged(int index);
    void onSelectRegion();
    void onStartStop();
    void onRecordingStarted();
    void onRecordingStopped(const QString &path);
    void onRecordingError(const QString &msg);
    void onProgressUpdated(int frames, double elapsed);
    void onBrowseOutput();
    void updateTimerDisplay();

private:
    void setupUi();
    void updatePresetInfo();
    void setRecordingState(bool recording);

    QComboBox *m_presetCombo;
    QLabel *m_resolutionLabel;
    QLabel *m_aspectLabel;
    QLabel *m_statusLabel;
    QLabel *m_timerLabel;
    QLabel *m_lastClipLabel;
    QPushButton *m_selectRegionBtn;
    QPushButton *m_recordBtn;
    QPushButton *m_browseBtn;
    QLineEdit *m_outputPath;
    QSpinBox *m_fpsSpin;
    QComboBox *m_qualityCombo;

    QTimer *m_timer;
    QRect m_captureRegion;
    std::unique_ptr<Recorder> m_recorder;
    PlatformPreset m_currentPreset;

    qint64 m_recordingStartMs = 0;
    int m_frameCount = 0;
    double m_elapsedSec = 0;
};
