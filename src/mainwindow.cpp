#include "mainwindow.h"
#include "recorder.h"
#include "regionselector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QApplication>
#include <QScreen>
#include <QMessageBox>
#include <QStyle>
#include <QFont>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_recorder(std::make_unique<Recorder>(this))
    , m_timer(new QTimer(this))
{
    setupUi();

    auto presets = platformPresets();
    m_currentPreset = presets[0];

    connect(m_recorder.get(), &Recorder::recordingStarted, this, &MainWindow::onRecordingStarted);
    connect(m_recorder.get(), &Recorder::recordingStopped, this, &MainWindow::onRecordingStopped);
    connect(m_recorder.get(), &Recorder::recordingError, this, &MainWindow::onRecordingError);
    connect(m_recorder.get(), &Recorder::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::updateTimerDisplay);

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::VideosLocation);
    m_outputPath->setText(defaultDir + "/clip_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".mp4");

    onPresetChanged(0);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("Clipper - Screen Recorder");
    setMinimumWidth(480);
    setStyleSheet(R"(
        QMainWindow { background: #1e1e2e; }
        QLabel { color: #cdd6f4; }
        QGroupBox { color: #89b4fa; font-weight: bold; border: 1px solid #45475a;
                     border-radius: 6px; margin-top: 12px; padding-top: 14px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
        QComboBox, QLineEdit, QSpinBox {
            background: #313244; color: #cdd6f4; border: 1px solid #45475a;
            border-radius: 4px; padding: 4px 8px;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background: #313244; color: #cdd6f4; selection-background-color: #89b4fa;
        }
        QPushButton {
            background: #89b4fa; color: #1e1e2e; border: none;
            border-radius: 4px; padding: 8px 16px; font-weight: bold;
        }
        QPushButton:hover { background: #a6c8ff; }
        QPushButton:pressed { background: #6c8ed4; }
        QPushButton#recordBtn { background: #f38ba8; font-size: 14px; padding: 10px 24px; }
    )");

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // --- Platform preset ---
    auto *presetGroup = new QGroupBox("Platform Preset");
    auto *presetLayout = new QFormLayout(presetGroup);

    m_presetCombo = new QComboBox();
    for (const auto &p : platformPresets())
        m_presetCombo->addItem(p.name, p.id);
    presetLayout->addRow("Format:", m_presetCombo);

    m_resolutionLabel = new QLabel();
    m_aspectLabel = new QLabel();
    presetLayout->addRow("Resolution:", m_resolutionLabel);
    presetLayout->addRow("Aspect Ratio:", m_aspectLabel);

    mainLayout->addWidget(presetGroup);

    // --- Capture settings ---
    auto *capGroup = new QGroupBox("Capture Settings");
    auto *capLayout = new QFormLayout(capGroup);

    m_fpsSpin = new QSpinBox();
    m_fpsSpin->setRange(10, 60);
    m_fpsSpin->setValue(30);
    m_fpsSpin->setSuffix(" fps");
    capLayout->addRow("Frame Rate:", m_fpsSpin);

    m_qualityCombo = new QComboBox();
    m_qualityCombo->addItem("High (8 Mbps)", 8000000);
    m_qualityCombo->addItem("Medium (4 Mbps)", 4000000);
    m_qualityCombo->addItem("Low (2 Mbps)", 2000000);
    capLayout->addRow("Quality:", m_qualityCombo);

    mainLayout->addWidget(capGroup);

    // --- Output ---
    auto *outGroup = new QGroupBox("Output");
    auto *outLayout = new QHBoxLayout(outGroup);

    m_outputPath = new QLineEdit();
    m_outputPath->setPlaceholderText("Output file path...");
    outLayout->addWidget(m_outputPath);

    m_browseBtn = new QPushButton("Browse");
    m_browseBtn->setFixedWidth(80);
    outLayout->addWidget(m_browseBtn);

    mainLayout->addWidget(outGroup);

    // --- Buttons ---
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_selectRegionBtn = new QPushButton("Select Region");
    m_selectRegionBtn->setMinimumHeight(36);
    m_selectRegionBtn->setStyleSheet("background: #45475a; color: #cdd6f4;");
    btnLayout->addWidget(m_selectRegionBtn);

    m_recordBtn = new QPushButton("Start Recording");
    m_recordBtn->setObjectName("recordBtn");
    m_recordBtn->setMinimumHeight(44);
    btnLayout->addWidget(m_recordBtn);

    mainLayout->addLayout(btnLayout);

    // --- Status ---
    auto *statusLayout = new QHBoxLayout();

    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setStyleSheet("color: #a6adc8;");
    statusLayout->addWidget(m_statusLabel);

    statusLayout->addStretch();

    m_timerLabel = new QLabel("00:00:00");
    m_timerLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #89b4fa;");
    m_timerLabel->setVisible(false);
    statusLayout->addWidget(m_timerLabel);

    mainLayout->addLayout(statusLayout);

    m_lastClipLabel = new QLabel();
    m_lastClipLabel->setStyleSheet("color: #a6e3a1; font-size: 11px;");
    m_lastClipLabel->setWordWrap(true);
    mainLayout->addWidget(m_lastClipLabel);

    // --- Connections ---
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPresetChanged);
    connect(m_selectRegionBtn, &QPushButton::clicked, this, &MainWindow::onSelectRegion);
    connect(m_recordBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
}

void MainWindow::onPresetChanged(int index)
{
    auto presets = platformPresets();
    if (index < 0 || index >= presets.size()) return;
    m_currentPreset = presets[index];
    updatePresetInfo();
}

void MainWindow::updatePresetInfo()
{
    m_resolutionLabel->setText(QString("%1 x %2")
        .arg(m_currentPreset.width)
        .arg(m_currentPreset.height));
    m_aspectLabel->setText(QString::number(m_currentPreset.aspectRatio(), 'f', 3) + ":1");

    // Default capture region to center of primary screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->geometry();
        int w = qMin(m_currentPreset.width, sg.width() / 2);
        int h = static_cast<int>(w / m_currentPreset.aspectRatio());
        if (h > sg.height() / 2) {
            h = sg.height() / 2;
            w = static_cast<int>(h * m_currentPreset.aspectRatio());
        }
        m_captureRegion = QRect(
            sg.x() + (sg.width() - w) / 2,
            sg.y() + (sg.height() - h) / 2,
            w, h);
    }

    m_statusLabel->setText(QString("Region: %1x%2 at (%3,%4) | Preset: %5")
        .arg(m_captureRegion.width()).arg(m_captureRegion.height())
        .arg(m_captureRegion.x()).arg(m_captureRegion.y())
        .arg(m_currentPreset.name));
}

void MainWindow::onSelectRegion()
{
    auto *selector = new RegionSelector(m_currentPreset.aspectRatio(), nullptr);
    selector->setAttribute(Qt::WA_DeleteOnClose);

    connect(selector, &RegionSelector::regionSelected, this, [this](QRect region) {
        m_captureRegion = region;
        m_statusLabel->setText(QString("Region: %1x%2 at (%3,%4) | Preset: %5")
            .arg(region.width()).arg(region.height())
            .arg(region.x()).arg(region.y())
            .arg(m_currentPreset.name));
    });

    connect(selector, &RegionSelector::cancelled, this, []() {
        // selection cancelled
    });

    hide();
    selector->show();
    connect(selector, &RegionSelector::destroyed, this, [this]() {
        show();
        raise();
        activateWindow();
    });
}

void MainWindow::onStartStop()
{
    if (m_recorder->isRecording()) {
        m_recorder->stopRecording();
        return;
    }

    QString outputPath = m_outputPath->text().trimmed();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, "No Output Path", "Please specify an output file path.");
        return;
    }

    // Ensure .mp4 extension
    if (!outputPath.endsWith(".mp4", Qt::CaseInsensitive))
        outputPath += ".mp4";

    int fps = m_fpsSpin->value();
    int bitrate = m_qualityCombo->currentData().toInt();

    m_recorder->startRecording(m_captureRegion, fps, bitrate, outputPath,
                               m_currentPreset.width, m_currentPreset.height);
}

void MainWindow::onRecordingStarted()
{
    m_errorShown = false;
    setRecordingState(true);
    m_recordingStartMs = QDateTime::currentMSecsSinceEpoch();
    m_timer->start(200);
    m_statusLabel->setText("Recording...");
}

void MainWindow::onRecordingStopped(const QString &path)
{
    setRecordingState(false);
    m_timer->stop();
    m_statusLabel->setText("Ready");

    if (!path.isEmpty()) {
        m_lastClipLabel->setText(QString("Saved: %1").arg(path));
    }
}

void MainWindow::onRecordingError(const QString &msg)
{
    if (m_errorShown) return;
    m_errorShown = true;
    setRecordingState(false);
    m_timer->stop();
    m_statusLabel->setText("Error: " + msg);
    m_statusLabel->setStyleSheet("color: #f38ba8;");
    QMessageBox::critical(this, "Recording Error", msg);
}

void MainWindow::onProgressUpdated(int frames, double elapsed)
{
    m_frameCount = frames;
    m_elapsedSec = elapsed;
}

void MainWindow::onBrowseOutput()
{
    QString dir = QFileDialog::getSaveFileName(
        this, "Save Recording As",
        m_outputPath->text(),
        "MP4 Video (*.mp4);;All Files (*)");
    if (!dir.isEmpty())
        m_outputPath->setText(dir);
}

void MainWindow::updateTimerDisplay()
{
    if (!m_recorder->isRecording()) return;

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_recordingStartMs;
    int secs = static_cast<int>(elapsed / 1000);
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    m_timerLabel->setText(QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

void MainWindow::setRecordingState(bool recording)
{
    if (recording) {
        m_recordBtn->setText("Stop Recording");
        m_recordBtn->setStyleSheet(
            "background: #a6e3a1; color: #1e1e2e; border: none; border-radius: 4px;"
            "padding: 10px 24px; font-weight: bold; font-size: 14px;");
        m_selectRegionBtn->setEnabled(false);
        m_presetCombo->setEnabled(false);
        m_outputPath->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_fpsSpin->setEnabled(false);
        m_qualityCombo->setEnabled(false);
        m_timerLabel->setVisible(true);
    } else {
        m_recordBtn->setText("Start Recording");
        m_recordBtn->setStyleSheet(
            "background: #f38ba8; color: #1e1e2e; border: none; border-radius: 4px;"
            "padding: 10px 24px; font-weight: bold; font-size: 14px;");
        m_selectRegionBtn->setEnabled(true);
        m_presetCombo->setEnabled(true);
        m_outputPath->setEnabled(true);
        m_browseBtn->setEnabled(true);
        m_fpsSpin->setEnabled(true);
        m_qualityCombo->setEnabled(true);
        m_timerLabel->setVisible(false);
    }

    m_statusLabel->setStyleSheet("color: #a6adc8;");
}
