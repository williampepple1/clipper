#include "hotkeymanager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QDebug>

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent)
{
}

HotkeyManager::~HotkeyManager()
{
    unregisterHotkey();
}

bool HotkeyManager::registerHotkey(const QKeySequence &keys)
{
#ifdef Q_OS_WIN
    if (m_hotkeyId != 0) return true;

    int key = keys[0].key();
    Qt::KeyboardModifiers mods = keys[0].keyboardModifiers();

    UINT fsModifiers = 0;
    if (mods & Qt::ShiftModifier)   fsModifiers |= MOD_SHIFT;
    if (mods & Qt::ControlModifier) fsModifiers |= MOD_CONTROL;
    if (mods & Qt::AltModifier)     fsModifiers |= MOD_ALT;
    if (mods & Qt::MetaModifier)    fsModifiers |= MOD_WIN;

    UINT vk = 0;
    // Map Qt key codes to Windows virtual keys
    switch (key) {
    case Qt::Key_A: case Qt::Key_B: case Qt::Key_C: case Qt::Key_D:
    case Qt::Key_E: case Qt::Key_F: case Qt::Key_G: case Qt::Key_H:
    case Qt::Key_I: case Qt::Key_J: case Qt::Key_K: case Qt::Key_L:
    case Qt::Key_M: case Qt::Key_N: case Qt::Key_O: case Qt::Key_P:
    case Qt::Key_Q: case Qt::Key_R: case Qt::Key_S: case Qt::Key_T:
    case Qt::Key_U: case Qt::Key_V: case Qt::Key_W: case Qt::Key_X:
    case Qt::Key_Y: case Qt::Key_Z:
        vk = static_cast<UINT>(key);
        break;
    case Qt::Key_F1: case Qt::Key_F2: case Qt::Key_F3: case Qt::Key_F4:
    case Qt::Key_F5: case Qt::Key_F6: case Qt::Key_F7: case Qt::Key_F8:
    case Qt::Key_F9: case Qt::Key_F10: case Qt::Key_F11: case Qt::Key_F12:
        vk = VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
        break;
    case Qt::Key_Space:  vk = VK_SPACE; break;
    case Qt::Key_Escape: vk = VK_ESCAPE; break;
    case Qt::Key_Tab:    vk = VK_TAB; break;
    default:
        qWarning() << "Unsupported hotkey:" << keys.toString();
        return false;
    }

    if (!::RegisterHotKey(reinterpret_cast<HWND>(0), HOTKEY_RECORD, fsModifiers, vk)) {
        qWarning() << "RegisterHotKey failed:" << GetLastError();
        return false;
    }

    m_hotkeyId = HOTKEY_RECORD;
    qDebug() << "Global hotkey registered:" << keys.toString();
    return true;
#else
    Q_UNUSED(keys);
    return false;
#endif
}

void HotkeyManager::unregisterHotkey()
{
#ifdef Q_OS_WIN
    if (m_hotkeyId != 0) {
        ::UnregisterHotKey(nullptr, m_hotkeyId);
        m_hotkeyId = 0;
    }
#endif
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == HOTKEY_RECORD) {
            emit hotkeyPressed();
            *result = 0;
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}
