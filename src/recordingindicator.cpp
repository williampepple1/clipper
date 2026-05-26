#include "recordingindicator.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

RecordingIndicator::RecordingIndicator(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    setFixedSize(200, 36);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move(sg.left() + 16, sg.top() + 16);
    }

    connect(m_timer, &QTimer::timeout, this, &RecordingIndicator::refresh);
}

void RecordingIndicator::start()
{
    m_startMs = QDateTime::currentMSecsSinceEpoch();
    m_blinkState = true;
    m_timer->start(500);
    show();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowLong(hwnd, GWL_EXSTYLE,
                  GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif

    raise();
}

void RecordingIndicator::stop()
{
    m_timer->stop();
    hide();
}

void RecordingIndicator::refresh()
{
    m_blinkState = !m_blinkState;
    update();
}

void RecordingIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background pill
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawRoundedRect(rect(), 18, 18);

    // Red dot
    QColor dotColor = m_blinkState ? QColor(255, 60, 80) : QColor(180, 40, 55);
    p.setBrush(dotColor);
    p.drawEllipse(10, 10, 16, 16);

    // Timer text
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    int secs = static_cast<int>(elapsed / 1000);
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    QString timeStr = QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));

    QFont f = font();
    f.setPointSize(10);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRect(32, 0, width() - 44, height()), Qt::AlignVCenter | Qt::AlignLeft, timeStr);

    // "REC" label
    QFont small = font();
    small.setPointSize(8);
    p.setFont(small);
    p.setPen(QColor(255, 180, 190));
    p.drawText(QRect(32, 0, width() - 44, height()), Qt::AlignVCenter | Qt::AlignRight, "REC");
}
