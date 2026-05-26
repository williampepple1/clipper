#pragma once

#include <QWidget>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QScreen>

class RegionSelector : public QWidget
{
    Q_OBJECT
public:
    explicit RegionSelector(double aspectRatio, QWidget *parent = nullptr);
    ~RegionSelector() override;

    QRect selectedRegion() const;
    double aspectRatio() const { return m_aspectRatio; }

signals:
    void regionSelected(QRect region);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class DragMode {
        None,
        Move,
        ResizeTL, ResizeTR, ResizeBL, ResizeBR,
        ResizeTop, ResizeBottom, ResizeLeft, ResizeRight
    };

    QRect clampToScreen(const QRect &r) const;
    void updateCursor(QMouseEvent *event);
    DragMode hitTest(const QPoint &pos) const;

    double m_aspectRatio;
    QRect m_selectionRect;
    QPoint m_dragStart;
    QRect m_dragStartRect;
    DragMode m_dragMode = DragMode::None;
    QScreen *m_screen = nullptr;

    static constexpr int HANDLE_SIZE = 10;
    static constexpr int MIN_SIZE = 160;
};
