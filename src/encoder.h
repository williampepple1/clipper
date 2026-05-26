#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <memory>

struct EncodeParams {
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int videoBitrate = 8000000;
    int sampleRate = 48000;
    int channels = 2;
    QString outputPath;
};

struct FramePacket {
    QImage image;
    qint64 timestampUs;
};

struct AudioPacket {
    QByteArray data;
    qint64 timestampUs;
};

class Encoder : public QObject
{
    Q_OBJECT
public:
    explicit Encoder(QObject *parent = nullptr);
    ~Encoder() override;

    bool initialize(const EncodeParams &params);
    void encodeLoop();
    void stopEncoding();
    bool isEncoding() const { return m_encoding; }

    void pushVideoFrame(const QImage &image, qint64 timestampUs);
    void pushAudioData(const QByteArray &data, qint64 timestampUs);

signals:
    void encodingStarted();
    void encodingStopped();
    void encodingError(const QString &message);
    void progressUpdated(int frameCount, double elapsedSec);

private:
    bool initVideoEncoder();
    bool initAudioEncoder();
    void finalize();

    EncodeParams m_params;

    QQueue<FramePacket> m_videoQueue;
    QQueue<AudioPacket> m_audioQueue;
    QMutex m_videoMutex;
    QMutex m_audioMutex;
    QWaitCondition m_videoCond;
    QWaitCondition m_audioCond;
    static constexpr int MAX_QUEUE_SIZE = 60;

    std::atomic<bool> m_encoding{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_finalized{false};
    bool m_streamsInitialized = false;

    void *m_fmtCtx = nullptr;
    void *m_videoCodecCtx = nullptr;
    void *m_audioCodecCtx = nullptr;
    void *m_swsCtx = nullptr;
    void *m_swrCtx = nullptr;
    void *m_videoStream = nullptr;
    void *m_audioStream = nullptr;
    void *m_videoFrame = nullptr;
    void *m_audioFrame = nullptr;

    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;
    int m_frameCount = 0;
    int64_t m_audioPts = 0;
};
