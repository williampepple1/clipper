#include "videotrimmer.h"
#include "timelineslider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QStyle>

extern "C" {
#include <libavformat/avformat.h>
}

VideoTrimmer::VideoTrimmer(const QString &videoPath, QWidget *parent)
    : QDialog(parent)
    , m_inputPath(videoPath)
{
    setWindowTitle("Trim Clip");
    setMinimumWidth(580);
    setStyleSheet(R"(
        QDialog { background: #FDF6EC; }
        QLabel { color: #4E342E; }
        QGroupBox { color: #5D4037; font-weight: bold; border: 1px solid #D7CCC8;
                     border-radius: 6px; margin-top: 12px; padding-top: 14px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
        QLineEdit {
            background: #FFF8F0; color: #4E342E; border: 1px solid #D7CCC8;
            border-radius: 4px; padding: 4px 8px;
        }
        QPushButton {
            background: #78909C; color: #FFFFFF; border: none;
            border-radius: 4px; padding: 8px 16px; font-weight: bold;
        }
        QPushButton:hover { background: #90A4AE; }
        QPushButton:pressed { background: #546E7A; }
        QPushButton#trimBtn { background: #81C784; color: #FFFFFF; padding: 10px 24px; font-size: 14px; }
        QPushButton#trimBtn:disabled { background: #D7CCC8; color: #A1887F; }
    )");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // File info
    auto *infoGroup = new QGroupBox("Video Info");
    auto *infoLayout = new QFormLayout(infoGroup);
    m_fileInfo = new QLabel();
    m_fileInfo->setWordWrap(true);
    infoLayout->addRow(m_fileInfo);
    m_durationLabel = new QLabel();
    infoLayout->addRow("Duration:", m_durationLabel);
    mainLayout->addWidget(infoGroup);

    // Timeline
    auto *timeGroup = new QGroupBox("Trim Range");
    auto *timeLayout = new QVBoxLayout(timeGroup);

    m_timeline = new TimelineSlider();
    timeLayout->addWidget(m_timeline);

    auto *timeLabels = new QHBoxLayout();
    m_startLabel = new QLabel("Start: 0:00");
    m_startLabel->setStyleSheet("color: #78909C; font-weight: bold;");
    timeLabels->addWidget(m_startLabel);
    timeLabels->addStretch();
    m_endLabel = new QLabel("End: 0:00");
    m_endLabel->setStyleSheet("color: #E8A0A0; font-weight: bold;");
    timeLabels->addWidget(m_endLabel);
    timeLayout->addLayout(timeLabels);

    mainLayout->addWidget(timeGroup);

    // Output
    auto *outGroup = new QGroupBox("Output");
    auto *outLayout = new QHBoxLayout(outGroup);
    m_outputEdit = new QLineEdit();
    m_outputEdit->setPlaceholderText("Trimmed output path...");
    outLayout->addWidget(m_outputEdit);

    m_browseBtn = new QPushButton("Browse");
    m_browseBtn->setFixedWidth(80);
    outLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(outGroup);

    // Action buttons
    auto *actionLayout = new QHBoxLayout();
    actionLayout->addStretch();

    m_statusLabel = new QLabel();
    actionLayout->addWidget(m_statusLabel);
    actionLayout->addStretch();

    m_trimBtn = new QPushButton("Trim & Export");
    m_trimBtn->setObjectName("trimBtn");
    m_trimBtn->setMinimumHeight(40);
    actionLayout->addWidget(m_trimBtn);

    mainLayout->addLayout(actionLayout);

    // Connections
    connect(m_timeline, &TimelineSlider::rangeChanged, this, &VideoTrimmer::onRangeChanged);
    connect(m_trimBtn, &QPushButton::clicked, this, &VideoTrimmer::onTrim);
    connect(m_browseBtn, &QPushButton::clicked, this, &VideoTrimmer::onBrowseOutput);

    loadVideoInfo();

    // Default output path
    QFileInfo fi(m_inputPath);
    m_outputPath = fi.absolutePath() + "/" + fi.completeBaseName() + "_trimmed.mp4";
    m_outputEdit->setText(m_outputPath);
}

VideoTrimmer::~VideoTrimmer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void VideoTrimmer::loadVideoInfo()
{
    AVFormatContext *ctx = nullptr;
    if (avformat_open_input(&ctx, m_inputPath.toUtf8().constData(), nullptr, nullptr) < 0) {
        m_fileInfo->setText("Failed to open: " + m_inputPath);
        m_durationSec = 0;
        return;
    }

    avformat_find_stream_info(ctx, nullptr);
    m_durationSec = ctx->duration / static_cast<double>(AV_TIME_BASE);

    int vidW = 0, vidH = 0;
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vidW = ctx->streams[i]->codecpar->width;
            vidH = ctx->streams[i]->codecpar->height;
            break;
        }
    }

    QFileInfo fi(m_inputPath);
    double fileMB = fi.size() / (1024.0 * 1024.0);

    m_fileInfo->setText(QString("%1  |  %2x%3  |  %4 MB")
        .arg(fi.fileName())
        .arg(vidW).arg(vidH)
        .arg(fileMB, 0, 'f', 1));

    avformat_close_input(&ctx);

    m_timeline->setDuration(m_durationSec);
    updateTimeLabels();
}

void VideoTrimmer::onRangeChanged(double start, double end)
{
    updateTimeLabels();
}

void VideoTrimmer::updateTimeLabels()
{
    auto fmt = [](double s) -> QString {
        int m = static_cast<int>(s) / 60;
        int sec = static_cast<int>(s) % 60;
        int ms = static_cast<int>((s - static_cast<int>(s)) * 1000);
        return QString("%1:%2.%3").arg(m).arg(sec, 2, 10, QChar('0')).arg(ms, 3, 10, QChar('0'));
    };

    m_startLabel->setText("Start: " + fmt(m_timeline->startTime()));
    m_endLabel->setText("End: " + fmt(m_timeline->endTime()));
    m_durationLabel->setText(QString("%1  (trim: %2)")
        .arg(fmt(m_durationSec))
        .arg(fmt(m_timeline->endTime() - m_timeline->startTime())));
}

void VideoTrimmer::onTrim()
{
    m_outputPath = m_outputEdit->text().trimmed();
    if (m_outputPath.isEmpty()) {
        QMessageBox::warning(this, "No Output Path", "Please specify an output file path.");
        return;
    }
    if (!m_outputPath.endsWith(".mp4", Qt::CaseInsensitive))
        m_outputPath += ".mp4";

    // Verify ffmpeg is available
    QProcess testProc;
    testProc.start("ffmpeg", {"-version"});
    testProc.waitForFinished(2000);
    if (testProc.exitCode() != 0) {
        QMessageBox::warning(this, "FFmpeg Not Found",
            "ffmpeg is not available on your system PATH.\n"
            "Install FFmpeg and ensure it's accessible from the command line.");
        return;
    }

    m_statusLabel->setText("Trimming...");
    m_statusLabel->setStyleSheet("color: #78909C;");
    m_trimBtn->setEnabled(false);

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoTrimmer::onProcessFinished);

    QStringList args;
    args << "-ss" << QString::number(m_timeline->startTime(), 'f', 3)
         << "-i" << m_inputPath
         << "-to" << QString::number(m_timeline->endTime() - m_timeline->startTime(), 'f', 3)
         << "-c" << "copy"
         << "-avoid_negative_ts" << "make_zero"
         << m_outputPath
         << "-y";

    m_process->start("ffmpeg", args);
}

void VideoTrimmer::onBrowseOutput()
{
    QString dir = QFileDialog::getSaveFileName(
        this, "Save Trimmed Video As",
        m_outputEdit->text(),
        "MP4 Video (*.mp4);;All Files (*)");
    if (!dir.isEmpty())
        m_outputEdit->setText(dir);
}

void VideoTrimmer::onProcessFinished(int exitCode)
{
    if (exitCode == 0) {
        m_statusLabel->setText("Done!");
        m_statusLabel->setStyleSheet("color: #81C784; font-weight: bold;");
        emit trimCompleted(m_outputPath);
    } else {
        m_statusLabel->setText("Failed");
        m_statusLabel->setStyleSheet("color: #D32F2F;");
        emit trimError("FFmpeg exited with code " + QString::number(exitCode));
    }
    m_process->deleteLater();
    m_process = nullptr;
}
