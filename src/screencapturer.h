#pragma once

#include <QObject>
#include <QRect>
#include <QImage>
#include <atomic>
#include <functional>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class ScreenCapturer : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCapturer(QObject *parent = nullptr);
    ~ScreenCapturer() override;

    bool initialize();
    void startCapture(const QRect &region, int targetFps);
    void stopCapture();
    bool isCapturing() const { return m_capturing; }

signals:
    void frameCaptured(QImage image, qint64 timestampUs);
    void captureError(const QString &message);
    void captureFinished();

private:
    ComPtr<ID3D11Device>        m_d3dDevice;
    ComPtr<ID3D11DeviceContext> m_d3dContext;
    ComPtr<IDXGIOutputDuplication> m_duplication;
    ComPtr<ID3D11Texture2D>     m_stagingTex;

    std::atomic<bool> m_capturing{false};
    std::atomic<bool> m_initialized{false};

    int m_outputWidth = 0;
    int m_outputHeight = 0;
};
