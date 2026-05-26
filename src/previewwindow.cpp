#include "previewwindow.h"
#include <QPainter>
#include <QCloseEvent>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>

PreviewWindow::PreviewWindow(QWidget *parent)
    : QWidget(parent)
    , m_refreshTimer(new QTimer(this))
{
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMinimumSize(240, 160);
    resize(320, 200);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move(sg.right() - width() - 16, sg.bottom() - height() - 16);
    }

    connect(m_refreshTimer, &QTimer::timeout, this, &PreviewWindow::refreshPreview);
}

void PreviewWindow::startPreview()
{
    m_active = true;
    m_refreshTimer->start(REFRESH_INTERVAL_MS);
    show();
}

void PreviewWindow::stopPreview()
{
    m_active = false;
    m_refreshTimer->stop();
    hide();
}

void PreviewWindow::updateFrame(QImage frame, qint64)
{
    if (!m_active) return;
    QMutexLocker lock(&m_frameMutex);
    m_latestFrame = frame.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
}

void PreviewWindow::refreshPreview()
{
    QMutexLocker lock(&m_frameMutex);
    if (!m_latestFrame.isNull())
        update();
}

void PreviewWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRoundedRect(rect(), 8, 8);

    QMutexLocker lock(&m_frameMutex);
    if (!m_latestFrame.isNull()) {
        QSize imgSize = m_latestFrame.size();
        QSize availSize = size() - QSize(16, 16);
        QSize targetSize = imgSize.scaled(availSize, Qt::KeepAspectRatio);
        QRect target(QPoint(0, 0), targetSize);
        target.moveCenter(rect().center());

        p.drawImage(target, m_latestFrame);

        // Border
        p.setPen(QPen(QColor(120, 144, 156, 100), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(target.adjusted(-1, -1, 1, 1), 4, 4);
    }

    // Title
    p.setPen(QColor("#E8D5C8"));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);
    p.drawText(rect().adjusted(10, 6, -10, 20), Qt::AlignLeft | Qt::AlignTop, "Preview");
}

void PreviewWindow::closeEvent(QCloseEvent *event)
{
    event->ignore(); // Don't close via X, only via stopPreview
}
