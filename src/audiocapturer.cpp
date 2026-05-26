#include "audiocapturer.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <audioclientactivationparams.h>

#pragma comment(lib, "avrt.lib")

namespace {
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
    const IID IID_IAudioClient = __uuidof(IAudioClient);
    const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
}

AudioCapturer::AudioCapturer(QObject *parent)
    : QObject(parent)
{
}

AudioCapturer::~AudioCapturer()
{
    stopCapture();
}

bool AudioCapturer::initialize(const AudioConfig &config)
{
    if (m_initialized) return true;
    m_config = config;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
        qWarning() << "CoInitializeEx failed:" << Qt::hex << hr;
        emit audioError("Failed to initialize COM for audio");
        return false;
    }

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator, (void **)m_enumerator.GetAddressOf());
    if (FAILED(hr)) {
        emit audioError("Failed to create audio device enumerator");
        return false;
    }

    hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, m_device.GetAddressOf());
    if (FAILED(hr)) {
        emit audioError("No audio output device found");
        return false;
    }

    hr = m_device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                            (void **)m_audioClient.GetAddressOf());
    if (FAILED(hr)) {
        emit audioError("Failed to activate audio client");
        return false;
    }

    WAVEFORMATEX *mixFormat = nullptr;
    hr = m_audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        emit audioError("Failed to get audio mix format");
        return false;
    }

    WAVEFORMATEX desiredFormat = {};
    desiredFormat.wFormatTag = WAVE_FORMAT_PCM;
    desiredFormat.nChannels = static_cast<WORD>(m_config.channels);
    desiredFormat.nSamplesPerSec = m_config.sampleRate;
    desiredFormat.wBitsPerSample = static_cast<WORD>(m_config.bitsPerSample);
    desiredFormat.nBlockAlign = desiredFormat.nChannels * desiredFormat.wBitsPerSample / 8;
    desiredFormat.nAvgBytesPerSec = desiredFormat.nSamplesPerSec * desiredFormat.nBlockAlign;
    desiredFormat.cbSize = 0;

    CoTaskMemFree(mixFormat);

    REFERENCE_TIME bufferDuration = 10000000;
    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_LOOPBACK,
                                   bufferDuration, 0, &desiredFormat, nullptr);
    if (FAILED(hr)) {
        qWarning() << "Audio client Initialize failed:" << Qt::hex << hr;
        emit audioError("Failed to initialize audio client in loopback mode");
        return false;
    }

    hr = m_audioClient->GetBufferSize(reinterpret_cast<UINT32 *>(&m_bufferFrames));
    if (FAILED(hr)) {
        emit audioError("Failed to get audio buffer size");
        return false;
    }

    hr = m_audioClient->GetService(IID_IAudioCaptureClient,
                                   (void **)m_captureClient.GetAddressOf());
    if (FAILED(hr)) {
        emit audioError("Failed to get audio capture client");
        return false;
    }

    m_initialized = true;
    return true;
}

void AudioCapturer::startCapture()
{
    if (!m_initialized) {
        emit audioError("Audio capturer not initialized");
        return;
    }
    if (m_capturing) return;

    m_capturing = true;

    QElapsedTimer clock;
    clock.start();

    HRESULT hr = m_audioClient->Start();
    if (FAILED(hr)) {
        emit audioError("Failed to start audio capture");
        m_capturing = false;
        emit audioFinished();
        return;
    }

    const int frameSize = m_config.channels * (m_config.bitsPerSample / 8);

    while (m_capturing) {
        UINT32 packetLength = 0;
        hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        while (packetLength > 0 && m_capturing) {
            uint8_t *data = nullptr;
            UINT32 framesAvailable = 0;
            DWORD flags = 0;
            UINT64 devicePosition = 0;
            UINT64 qpcPosition = 0;

            hr = m_captureClient->GetBuffer(&data, &framesAvailable, &flags,
                                            &devicePosition, &qpcPosition);
            if (FAILED(hr)) break;

            if (data && framesAvailable > 0 && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                int byteCount = static_cast<int>(framesAvailable * frameSize);
                qint64 timestampUs = clock.nsecsElapsed() / 1000;
                emit audioCaptured(
                    QByteArray(reinterpret_cast<const char *>(data), byteCount),
                    timestampUs);
            }

            hr = m_captureClient->ReleaseBuffer(framesAvailable);
            if (FAILED(hr)) break;

            hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }

        QThread::msleep(5);
    }

    m_audioClient->Stop();
    m_audioClient.Reset();
    m_captureClient.Reset();
    m_device.Reset();
    m_enumerator.Reset();

    emit audioFinished();
}

void AudioCapturer::stopCapture()
{
    m_capturing = false;
}
