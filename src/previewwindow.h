#pragma once

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QMutex>

class PreviewWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewWindow(QWidget *parent = nullptr);

    void startPreview();
    void stopPreview();

public slots:
    void updateFrame(QImage frame, qint64 timestampUs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshPreview();

private:
    QImage m_latestFrame;
    QMutex m_frameMutex;
    QTimer *m_refreshTimer;
    bool m_active = false;
    qint64 m_lastRefreshUs = 0;
    static constexpr int REFRESH_INTERVAL_MS = 66; // ~15 fps
};
