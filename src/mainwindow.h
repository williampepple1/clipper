#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QCheckBox>
#include <QRect>
#include <QImage>
#include <memory>

#include "platformpresets.h"

class Recorder;
class HotkeyManager;
class PreviewWindow;
class RecordingIndicator;

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
    void onHotkeyPressed();
    void onPreviewFrame(QImage frame);
    void openTrimmer(const QString &path);

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
    QCheckBox *m_previewCb;

    QTimer *m_timer;
    QRect m_captureRegion;
    std::unique_ptr<Recorder> m_recorder;
    PlatformPreset m_currentPreset;
    HotkeyManager *m_hotkeyMgr = nullptr;
    std::unique_ptr<PreviewWindow>     m_previewWindow;
    std::unique_ptr<RecordingIndicator> m_indicator;

    qint64 m_recordingStartMs = 0;
    int m_frameCount = 0;
    double m_elapsedSec = 0;
    bool m_errorShown = false;
};
