#include "timelineslider.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

TimelineSlider::TimelineSlider(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(60);
    setMouseTracking(true);
}

void TimelineSlider::setDuration(double seconds)
{
    m_duration = qMax(0.1, seconds);
    m_startSec = 0;
    m_endSec = m_duration;
    update();
}

void TimelineSlider::setRange(double startSec, double endSec)
{
    m_startSec = qBound(0.0, startSec, m_duration);
    m_endSec = qBound(m_startSec + 0.1, endSec, m_duration);
    update();
}

void TimelineSlider::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int barY = height() / 2;
    int barH = 8;
    int barLeft = MARGIN;
    int barRight = width() - MARGIN;
    int barW = barRight - barLeft;

    // Background track
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#E0D6C8"));

    // Selected range
    int selLeft = timeToPos(m_startSec);
    int selRight = timeToPos(m_endSec);
    p.setBrush(QColor(120, 144, 156, 80));

    // Tick marks
    p.setPen(QColor(141, 110, 99, 80));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);
    int tickCount = qMin(10, qMax(2, static_cast<int>(m_duration)));
    for (int i = 0; i <= tickCount; ++i) {
        double t = (m_duration / tickCount) * i;
        int x = timeToPos(t);
        p.drawLine(x, barY - barH / 2 - 6, x, barY - barH / 2);
        p.drawLine(x, barY + barH / 2, x, barY + barH / 2 + 6);

        int mins = static_cast<int>(t) / 60;
        int secs = static_cast<int>(t) % 60;
        p.drawText(QRect(x - 30, barY + barH / 2 + 8, 60, 16),
                   Qt::AlignCenter, QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
    }

    // Start handle (left)
    QRect startR(selLeft - HANDLE_WIDTH / 2, barY - 14, HANDLE_WIDTH, 28);
    p.setBrush(QColor("#78909C"));
    p.setPen(QPen(QColor("#FDF6EC"), 2));
    p.drawRoundedRect(startR, 4, 4);

    // End handle (right)
    QRect endR(selRight - HANDLE_WIDTH / 2, barY - 14, HANDLE_WIDTH, 28);
    p.setBrush(QColor("#E8A0A0"));
    p.setPen(QPen(QColor("#FDF6EC"), 2));
    p.drawRoundedRect(endR, 4, 4);
}

void TimelineSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = hitTest(event->pos());
    }
}

void TimelineSlider::mouseMoveEvent(QMouseEvent *event)
{
    Handle hover = hitTest(event->pos());
    if (hover == StartHandle || hover == EndHandle)
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::ArrowCursor);

    if (m_dragging == None) return;

    double t = posToTime(event->pos().x());
    if (m_dragging == StartHandle) {
        m_startSec = qBound(0.0, t, m_endSec - 0.1);
    } else if (m_dragging == EndHandle) {
        m_endSec = qBound(m_startSec + 0.1, t, m_duration);
    }
    update();
    emit rangeChanged(m_startSec, m_endSec);
}

void TimelineSlider::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = None;
    emit rangeChanged(m_startSec, m_endSec);
}

int TimelineSlider::timeToPos(double sec) const
{
    if (m_duration <= 0) return MARGIN;
    int barW = width() - 2 * MARGIN;
    return MARGIN + static_cast<int>((sec / m_duration) * barW);
}

double TimelineSlider::posToTime(int x) const
{
    int barW = width() - 2 * MARGIN;
    double frac = static_cast<double>(x - MARGIN) / barW;
    return qBound(0.0, frac * m_duration, m_duration);
}

TimelineSlider::Handle TimelineSlider::hitTest(const QPoint &pos) const
{
    int sx = timeToPos(m_startSec);
    int ex = timeToPos(m_endSec);
    if (qAbs(pos.x() - sx) < HANDLE_WIDTH && qAbs(pos.y() - height() / 2) < 20)
        return StartHandle;
    if (qAbs(pos.x() - ex) < HANDLE_WIDTH && qAbs(pos.y() - height() / 2) < 20)
        return EndHandle;
    return None;
}
