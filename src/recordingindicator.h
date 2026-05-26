#pragma once

#include <QWidget>
#include <QTimer>
#include <QDateTime>

class RecordingIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit RecordingIndicator(QWidget *parent = nullptr);

    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void refresh();

private:
    QTimer *m_timer;
    qint64 m_startMs = 0;
    bool m_blinkState = false;
};
