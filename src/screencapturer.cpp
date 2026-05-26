#include "screencapturer.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent)
{
}

ScreenCapturer::~ScreenCapturer()
{
    stopCapture();
}

bool ScreenCapturer::initialize()
{
    if (m_initialized) return true;

    D3D_FEATURE_LEVEL featLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        m_d3dDevice.GetAddressOf(), &featLevel, m_d3dContext.GetAddressOf());

    if (FAILED(hr)) {
        qWarning() << "D3D11CreateDevice failed:" << Qt::hex << hr;
        emit captureError("Failed to create D3D11 device");
        return false;
    }

    ComPtr<IDXGIDevice> dxgiDev;
    hr = m_d3dDevice.As(&dxgiDev);
    if (FAILED(hr)) {
        emit captureError("Failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDev->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        emit captureError("Failed to get graphics adapter");
        return false;
    }

    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(0, output.GetAddressOf());
    if (FAILED(hr)) {
        emit captureError("Failed to get display output");
        return false;
    }

    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        emit captureError("Display output does not support duplication");
        return false;
    }

    hr = output1->DuplicateOutput(m_d3dDevice.Get(), m_duplication.GetAddressOf());
    if (FAILED(hr)) {
        qWarning() << "DuplicateOutput failed:" << Qt::hex << hr;
        if (hr == E_ACCESSDENIED)
            emit captureError("Access denied. Close other screen recorders or run on a non-protected display.");
        else
            emit captureError("Failed to duplicate output");
        return false;
    }

    DXGI_OUTDUPL_DESC dupDesc;
    m_duplication->GetDesc(&dupDesc);
    qDebug() << "Desktop duplication ready:" << dupDesc.ModeDesc.Width << "x" << dupDesc.ModeDesc.Height;

    m_initialized = true;
    return true;
}

void ScreenCapturer::startCapture(const QRect &region, int targetFps)
{
    if (!m_initialized) {
        emit captureError("Capturer not initialized");
        return;
    }
    if (m_capturing) return;

    m_outputWidth = region.width();
    m_outputHeight = region.height();
    m_capturing = true;

    const int frameIntervalUs = (targetFps > 0) ? (1000000 / targetFps) : 33333;
    QElapsedTimer clock;
    clock.start();
    qint64 lastCaptureUs = 0;

    while (m_capturing) {
        qint64 nowUs = clock.nsecsElapsed() / 1000;
        qint64 elapsed = nowUs - lastCaptureUs;

        if (elapsed < frameIntervalUs) {
            QThread::usleep(static_cast<unsigned long>(frameIntervalUs - elapsed));
            nowUs = clock.nsecsElapsed() / 1000;
        }
        lastCaptureUs = nowUs;

        ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        HRESULT hr = m_duplication->AcquireNextFrame(
            static_cast<UINT>(frameIntervalUs * 2),
            &frameInfo, desktopResource.GetAddressOf());

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) continue;

        if (FAILED(hr)) {
            if (hr == DXGI_ERROR_ACCESS_LOST) {
                qWarning() << "Access lost, reinitializing duplication...";
                m_duplication.Reset();
                emit captureError("Desktop capture access lost");
            }
            continue;
        }

        ComPtr<ID3D11Texture2D> srcTex;
        hr = desktopResource.As(&srcTex);
        if (FAILED(hr)) {
            m_duplication->ReleaseFrame();
            continue;
        }

        D3D11_TEXTURE2D_DESC srcDesc;
        srcTex->GetDesc(&srcDesc);

        if (!m_stagingTex) {
            D3D11_TEXTURE2D_DESC stagingDesc = {};
            stagingDesc.Width = srcDesc.Width;
            stagingDesc.Height = srcDesc.Height;
            stagingDesc.MipLevels = 1;
            stagingDesc.ArraySize = 1;
            stagingDesc.Format = srcDesc.Format;
            stagingDesc.SampleDesc.Count = 1;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            hr = m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, m_stagingTex.GetAddressOf());
            if (FAILED(hr)) {
                m_duplication->ReleaseFrame();
                continue;
            }
        }

        m_d3dContext->CopyResource(m_stagingTex.Get(), srcTex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = m_d3dContext->Map(m_stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            int cropX = qBound(0, region.x(), (int)srcDesc.Width);
            int cropY = qBound(0, region.y(), (int)srcDesc.Height);
            int cropW = qMin(region.width(), (int)srcDesc.Width - cropX);
            int cropH = qMin(region.height(), (int)srcDesc.Height - cropY);

            if (cropW > 0 && cropH > 0) {
                QImage img(cropW, cropH, QImage::Format_ARGB32);
                uint8_t *srcRow = static_cast<uint8_t *>(mapped.pData)
                    + (cropY * mapped.RowPitch) + (cropX * 4);

                for (int y = 0; y < cropH; ++y) {
                    uint32_t *src = reinterpret_cast<uint32_t *>(srcRow + y * mapped.RowPitch);
                    QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));
                    for (int x = 0; x < cropW; ++x) {
                        uint32_t pixel = src[x];
                        uint8_t b = (pixel >> 16) & 0xFF;
                        uint8_t g = (pixel >> 8) & 0xFF;
                        uint8_t r = pixel & 0xFF;
                        dst[x] = qRgb(r, g, b);
                    }
                }

                if (cropW != m_outputWidth || cropH != m_outputHeight) {
                    img = img.scaled(m_outputWidth, m_outputHeight,
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }

                emit frameCaptured(img, nowUs);
            }

            m_d3dContext->Unmap(m_stagingTex.Get(), 0);
        }

        m_duplication->ReleaseFrame();
    }

    m_stagingTex.Reset();
    m_duplication.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();

    emit captureFinished();
}

void ScreenCapturer::stopCapture()
{
    m_capturing = false;
}
