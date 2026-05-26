#pragma once

#include <QWidget>
#include <QPoint>

class TimelineSlider : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineSlider(QWidget *parent = nullptr);

    void setDuration(double seconds);
    void setRange(double startSec, double endSec);
    double startTime() const { return m_startSec; }
    double endTime() const { return m_endSec; }

signals:
    void rangeChanged(double startSec, double endSec);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum Handle { None, StartHandle, EndHandle };
    Handle hitTest(const QPoint &pos) const;
    int timeToPos(double sec) const;
    double posToTime(int x) const;

    double m_duration = 0;
    double m_startSec = 0;
    double m_endSec = 0;
    Handle m_dragging = None;
    static constexpr int HANDLE_WIDTH = 12;
    static constexpr int MARGIN = 20;
};
