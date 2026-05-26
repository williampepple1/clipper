#include "regionselector.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QCursor>
#include <QGuiApplication>

RegionSelector::RegionSelector(double aspectRatio, QWidget *parent)
    : QWidget(parent)
    , m_aspectRatio(aspectRatio)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);

    QRect total;
    for (auto *screen : QGuiApplication::screens()) {
        total = total.united(screen->geometry());
    }
    setGeometry(total);

    int initW = qMin(800, total.width() / 2);
    int initH = static_cast<int>(initW / m_aspectRatio);
    if (initH > total.height() / 2) {
        initH = total.height() / 2;
        initW = static_cast<int>(initH * m_aspectRatio);
    }
    int cx = (total.width() - initW) / 2;
    int cy = (total.height() - initH) / 2;
    m_selectionRect = QRect(cx, cy, initW, initH);
    m_selectionRect = clampToScreen(m_selectionRect);
}

RegionSelector::~RegionSelector() = default;

QRect RegionSelector::selectedRegion() const
{
    return m_selectionRect;
}

void RegionSelector::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dim the background
    p.fillRect(rect(), QColor(0, 0, 0, 120));

    // Cutout the selection
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(m_selectionRect, Qt::black);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Selection border
    QPen border(QColor(0, 150, 255), 2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRect(m_selectionRect.adjusted(1, 1, -1, -1));

    // Outer glow
    QPen glow(QColor(0, 150, 255, 80), 4);
    p.setPen(glow);
    p.drawRect(m_selectionRect.adjusted(2, 2, -2, -2));

    // Corner handles
    p.setBrush(QColor(0, 150, 255));
    p.setPen(Qt::NoBrush);
    int hs = HANDLE_SIZE;
    p.drawRect(m_selectionRect.left(), m_selectionRect.top(), hs, hs);
    p.drawRect(m_selectionRect.right() - hs + 1, m_selectionRect.top(), hs, hs);
    p.drawRect(m_selectionRect.left(), m_selectionRect.bottom() - hs + 1, hs, hs);
    p.drawRect(m_selectionRect.right() - hs + 1, m_selectionRect.bottom() - hs + 1, hs, hs);

    // Edge handles
    int mw = m_selectionRect.width() / 2 - hs / 2;
    p.drawRect(m_selectionRect.left() + mw, m_selectionRect.top(), hs, hs);
    p.drawRect(m_selectionRect.left() + mw, m_selectionRect.bottom() - hs + 1, hs, hs);
    int mh = m_selectionRect.height() / 2 - hs / 2;
    p.drawRect(m_selectionRect.left(), m_selectionRect.top() + mh, hs, hs);
    p.drawRect(m_selectionRect.right() - hs + 1, m_selectionRect.top() + mh, hs, hs);

    // Center crosshair
    p.setPen(QColor(255, 255, 255, 60));
    int cx = m_selectionRect.center().x();
    int cy = m_selectionRect.center().y();
    p.drawLine(cx - 10, cy, cx + 10, cy);
    p.drawLine(cx, cy - 10, cx, cy + 10);

    // Info text
    QFont font("Segoe UI", 11);
    p.setFont(font);
    QString info = QString("%1 x %2  |  %3:1  |  Enter to confirm, Esc to cancel")
                       .arg(m_selectionRect.width())
                       .arg(m_selectionRect.height())
                       .arg(m_aspectRatio, 0, 'f', 2);
    QRect textRect = p.boundingRect(QRect(), Qt::AlignLeft, info);
    QRect bgRect = textRect.adjusted(-8, -4, 8, 4);
    bgRect.moveTopLeft(QPoint(m_selectionRect.left(), m_selectionRect.bottom() + 8));
    if (bgRect.bottom() > height()) {
        bgRect.moveBottomLeft(QPoint(m_selectionRect.left(), m_selectionRect.top() - 8));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawRoundedRect(bgRect, 4, 4);
    p.setPen(Qt::white);
    p.drawText(bgRect, Qt::AlignCenter, info);
}

void RegionSelector::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragMode = hitTest(event->pos());
        m_dragStart = event->pos();
        m_dragStartRect = m_selectionRect;
    }
}

void RegionSelector::mouseMoveEvent(QMouseEvent *event)
{
    updateCursor(event);

    if (m_dragMode == DragMode::None) return;

    QPoint delta = event->pos() - m_dragStart;
    QRect r = m_dragStartRect;

    switch (m_dragMode) {
    case DragMode::Move:
        r.translate(delta);
        break;
    case DragMode::ResizeTL:
        r.setTopLeft(r.topLeft() + delta);
        r = clampToScreen(r);
        r.setHeight(static_cast<int>(r.width() / m_aspectRatio));
        break;
    case DragMode::ResizeBR:
        r.setBottomRight(r.bottomRight() + delta);
        r.setWidth(static_cast<int>(r.height() * m_aspectRatio));
        break;
    case DragMode::ResizeTR:
        r.setTopRight(r.topRight() + delta);
        r = clampToScreen(r);
        r.setHeight(static_cast<int>(r.width() / m_aspectRatio));
        break;
    case DragMode::ResizeBL:
        r.setBottomLeft(r.bottomLeft() + delta);
        r = clampToScreen(r);
        r.setHeight(static_cast<int>(r.width() / m_aspectRatio));
        break;
    case DragMode::ResizeTop:
        r.setTop(r.top() + delta.y());
        r = clampToScreen(r);
        r.setHeight(static_cast<int>(r.width() / m_aspectRatio));
        break;
    case DragMode::ResizeBottom:
        r.setBottom(r.bottom() + delta.y());
        r.setWidth(static_cast<int>(r.height() * m_aspectRatio));
        break;
    case DragMode::ResizeLeft:
        r.setLeft(r.left() + delta.x());
        r = clampToScreen(r);
        r.setHeight(static_cast<int>(r.width() / m_aspectRatio));
        break;
    case DragMode::ResizeRight:
        r.setRight(r.right() + delta.x());
        r.setWidth(static_cast<int>(r.height() * m_aspectRatio));
        break;
    default:
        break;
    }

    r = clampToScreen(r);

    if (r.width() >= MIN_SIZE && r.height() >= MIN_SIZE) {
        m_selectionRect = r;
        update();
    }
}

void RegionSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragMode = DragMode::None;
    }
}

void RegionSelector::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit regionSelected(m_selectionRect);
        close();
    } else if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        close();
    }
}

QRect RegionSelector::clampToScreen(const QRect &r) const
{
    QRect screen = geometry();
    QRect clamped = r;
    if (clamped.left() < 0) clamped.moveLeft(0);
    if (clamped.top() < 0) clamped.moveTop(0);
    if (clamped.right() > screen.width()) clamped.moveRight(screen.width());
    if (clamped.bottom() > screen.height()) clamped.moveBottom(screen.height());
    return clamped;
}

void RegionSelector::updateCursor(QMouseEvent *event)
{
    switch (hitTest(event->pos())) {
    case DragMode::Move:
        setCursor(Qt::SizeAllCursor);
        break;
    case DragMode::ResizeTL:
    case DragMode::ResizeBR:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case DragMode::ResizeTR:
    case DragMode::ResizeBL:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case DragMode::ResizeTop:
    case DragMode::ResizeBottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case DragMode::ResizeLeft:
    case DragMode::ResizeRight:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        setCursor(Qt::ArrowCursor);
        break;
    }
}

RegionSelector::DragMode RegionSelector::hitTest(const QPoint &pos) const
{
    int hs = HANDLE_SIZE + 4;
    QRect r = m_selectionRect;

    // Corners
    if (QRect(r.left(), r.top(), hs, hs).contains(pos)) return DragMode::ResizeTL;
    if (QRect(r.right() - hs + 1, r.top(), hs, hs).contains(pos)) return DragMode::ResizeTR;
    if (QRect(r.left(), r.bottom() - hs + 1, hs, hs).contains(pos)) return DragMode::ResizeBL;
    if (QRect(r.right() - hs + 1, r.bottom() - hs + 1, hs, hs).contains(pos)) return DragMode::ResizeBR;

    // Edges
    int mw = r.width() / 2 - hs / 2;
    if (QRect(r.left() + mw, r.top(), hs, hs).contains(pos)) return DragMode::ResizeTop;
    if (QRect(r.left() + mw, r.bottom() - hs + 1, hs, hs).contains(pos)) return DragMode::ResizeBottom;
    int mh = r.height() / 2 - hs / 2;
    if (QRect(r.left(), r.top() + mh, hs, hs).contains(pos)) return DragMode::ResizeLeft;
    if (QRect(r.right() - hs + 1, r.top() + mh, hs, hs).contains(pos)) return DragMode::ResizeRight;

    if (r.contains(pos)) return DragMode::Move;
    return DragMode::None;
}
