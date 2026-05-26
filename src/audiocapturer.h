#pragma once

#include <QObject>
#include <QByteArray>
#include <atomic>

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct AudioConfig {
    int sampleRate = 48000;
    int channels = 2;
    int bitsPerSample = 16;
};

class AudioCapturer : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapturer(QObject *parent = nullptr);
    ~AudioCapturer() override;

    bool initialize(const AudioConfig &config = AudioConfig());
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return m_capturing; }
    AudioConfig config() const { return m_config; }

signals:
    void audioCaptured(QByteArray data, qint64 timestampUs);
    void audioError(const QString &message);
    void audioFinished();

private:
    ComPtr<IMMDeviceEnumerator> m_enumerator;
    ComPtr<IMMDevice>           m_device;
    ComPtr<IAudioClient>        m_audioClient;
    ComPtr<IAudioCaptureClient> m_captureClient;

    AudioConfig m_config;
    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_initialized{false};
    int m_bufferFrames = 0;
};
