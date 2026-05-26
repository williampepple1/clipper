#pragma once

#include <QObject>
#include <QString>
#include <QRect>
#include <QThread>
#include <atomic>
#include <memory>

class ScreenCapturer;
class AudioCapturer;
class Encoder;

class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool startRecording(const QRect &captureRegion, int fps, int videoBitrate,
                        const QString &outputPath, int outputWidth, int outputHeight);
    void stopRecording();
    bool isRecording() const;

signals:
    void recordingStarted();
    void recordingStopped(const QString &outputPath);
    void recordingError(const QString &message);
    void progressUpdated(int frameCount, double elapsedSec);

private slots:
    void onEncoderStopped();

private:
    void cleanupThreads();

    std::unique_ptr<ScreenCapturer> m_capturer;
    std::unique_ptr<AudioCapturer>  m_audioCapturer;
    std::unique_ptr<Encoder>        m_encoder;
    std::unique_ptr<QThread>        m_capThread;
    std::unique_ptr<QThread>        m_audioThread;
    std::unique_ptr<QThread>        m_encThread;

    std::atomic<bool> m_recording{false};
    int m_outputWidth = 1920;
    int m_outputHeight = 1080;
    qreal m_dpiScale = 1.0;
    QString m_outputPath;
};
