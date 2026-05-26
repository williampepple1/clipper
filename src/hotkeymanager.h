#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>

class HotkeyManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    bool registerHotkey(const QKeySequence &keys);
    void unregisterHotkey();

signals:
    void hotkeyPressed();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    int m_hotkeyId = 0;
    static constexpr int HOTKEY_RECORD = 1;
};
