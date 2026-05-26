#include "recorder.h"
#include "screencapturer.h"
#include "audiocapturer.h"
#include "encoder.h"
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

Recorder::Recorder(QObject *parent)
    : QObject(parent)
{
}

Recorder::~Recorder()
{
    if (m_encoder) {
        QObject::disconnect(m_encoder.get(), nullptr, this, nullptr);
    }
    stopRecording();
    cleanupThreads();
}

bool Recorder::startRecording(const QRect &captureRegion, int fps, int videoBitrate,
                               const QString &outputPath, int outputWidth, int outputHeight)
{
    if (m_recording.load()) {
        emit recordingError("Already recording");
        return false;
    }

    m_outputWidth = outputWidth;
    m_outputHeight = outputHeight;
    m_outputPath = outputPath;

    // Scale logical capture region to physical pixels for HiDPI displays
    m_dpiScale = 1.0;
    if (auto *screen = QGuiApplication::primaryScreen())
        m_dpiScale = screen->devicePixelRatio();
    QRect physRegion(
        qRound(captureRegion.x() * m_dpiScale),
        qRound(captureRegion.y() * m_dpiScale),
        qRound(captureRegion.width() * m_dpiScale),
        qRound(captureRegion.height() * m_dpiScale));

    cleanupThreads();

    m_capturer = std::make_unique<ScreenCapturer>();
    m_audioCapturer = std::make_unique<AudioCapturer>();
    m_encoder = std::make_unique<Encoder>();

    m_capThread = std::make_unique<QThread>();
    m_audioThread = std::make_unique<QThread>();
    m_encThread = std::make_unique<QThread>();

    auto *capturer = m_capturer.get();
    auto *audioCapturer = m_audioCapturer.get();
    auto *encoder = m_encoder.get();

    // Move to threads
    capturer->moveToThread(m_capThread.get());
    audioCapturer->moveToThread(m_audioThread.get());
    encoder->moveToThread(m_encThread.get());

    // Encoder init params
    EncodeParams params;
    params.width = outputWidth;
    params.height = outputHeight;
    params.fps = fps;
    params.videoBitrate = videoBitrate;
    params.sampleRate = 48000;
    params.channels = 2;
    params.outputPath = outputPath;

    // Forward signals from encoder
    connect(encoder, &Encoder::encodingError, this, &Recorder::recordingError);
    connect(encoder, &Encoder::progressUpdated, this, &Recorder::progressUpdated);
    connect(encoder, &Encoder::encodingStarted, this, &Recorder::recordingStarted);
    connect(encoder, &Encoder::encodingStopped, this, &Recorder::onEncoderStopped);

    // Frame pipeline: capturers -> encoder (DirectConnection since encoder's
    // queue is thread-safe, and threads don't run event loops)
    connect(capturer, &ScreenCapturer::frameCaptured, encoder,
            [encoder](const QImage &img, qint64 ts) {
                encoder->pushVideoFrame(img, ts);
            }, Qt::DirectConnection);

    connect(audioCapturer, &AudioCapturer::audioCaptured, encoder,
            [encoder](const QByteArray &data, qint64 ts) {
                encoder->pushAudioData(data, ts);
            }, Qt::DirectConnection);

    // Forward frames for real-time preview (AutoConnection -> main thread)
    connect(capturer, &ScreenCapturer::frameCaptured, this, &Recorder::previewFrameAvailable,
            Qt::AutoConnection);

    // Forward capturer errors - AutoConnection ensures main-thread delivery
    connect(capturer, &ScreenCapturer::captureError, this, [this](const QString &msg) {
        if (m_recording.load()) {
            emit recordingError(msg);
            stopRecording();
        }
    });

    connect(audioCapturer, &AudioCapturer::audioError, this, [this](const QString &msg) {
        if (m_recording.load()) {
            emit recordingError(msg);
            stopRecording();
        }
    });

    // When capturer finishes, stop encoder too
    connect(capturer, &ScreenCapturer::captureFinished, encoder, &Encoder::stopEncoding,
            Qt::DirectConnection);

    // Set up thread execution - each thread runs the worker's loop
    connect(m_capThread.get(), &QThread::started, capturer, [capturer, physRegion, fps]() {
        capturer->initialize();
        capturer->startCapture(physRegion, fps);
    });

    connect(m_audioThread.get(), &QThread::started, audioCapturer, [audioCapturer]() {
        audioCapturer->initialize();
        audioCapturer->startCapture();
    });

    connect(m_encThread.get(), &QThread::started, encoder, [encoder, params]() {
        encoder->initialize(params);
        encoder->encodeLoop();
    });

    // Clean up threads when done (but keep objects alive until Recorder destructor)
    connect(capturer, &ScreenCapturer::captureFinished, m_capThread.get(), &QThread::quit);
    connect(audioCapturer, &AudioCapturer::audioFinished, m_audioThread.get(), &QThread::quit);

    m_capThread->start();
    m_audioThread->start();
    m_encThread->start();

    m_recording.store(true);
    return true;
}

void Recorder::stopRecording()
{
    if (!m_recording.load()) return;

    if (m_capturer) m_capturer->stopCapture();
    if (m_audioCapturer) m_audioCapturer->stopCapture();
    if (m_encoder) m_encoder->stopEncoding();
}

bool Recorder::isRecording() const
{
    return m_recording.load();
}

void Recorder::onEncoderStopped()
{
    m_recording.store(false);
    emit recordingStopped(m_outputPath);
    cleanupThreads();
}

void Recorder::cleanupThreads()
{
    if (m_capThread && m_capThread->isRunning()) {
        m_capThread->quit();
        m_capThread->wait(3000);
    }
    if (m_audioThread && m_audioThread->isRunning()) {
        m_audioThread->quit();
        m_audioThread->wait(3000);
    }
    if (m_encThread && m_encThread->isRunning()) {
        m_encThread->quit();
        m_encThread->wait(5000);
    }

    m_capThread.reset();
    m_audioThread.reset();
    m_encThread.reset();
    m_capturer.reset();
    m_audioCapturer.reset();
    m_encoder.reset();
}
