#include "hotkey_manager.h"
#include "debug_log.h"
#include <QCoreApplication>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    viperDbg("HK1 installNativeEventFilter\n");
    qApp->installNativeEventFilter(this);
    viperDbg("HK2 done\n");
}

HotkeyManager::~HotkeyManager() {
    for (int id : m_registeredIds)
        unregisterHotkey(id);
    qApp->removeNativeEventFilter(this);
}

bool HotkeyManager::registerHotkey(int id, Qt::Key key, Qt::KeyboardModifiers mods) {
#ifdef Q_OS_WIN
    UINT winMods = 0;
    if (mods & Qt::ControlModifier) winMods |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)   winMods |= MOD_SHIFT;
    if (mods & Qt::AltModifier)     winMods |= MOD_ALT;
    if (mods & Qt::MetaModifier)    winMods |= MOD_WIN;

    // Map Qt::Key to VK code (basic mapping for printable keys)
    UINT vk = static_cast<UINT>(key);

    if (RegisterHotKey(nullptr, id, winMods, vk)) {
        m_registeredIds.append(id);
        return true;
    }
#else
    Q_UNUSED(id) Q_UNUSED(key) Q_UNUSED(mods)
#endif
    return false;
}

void HotkeyManager::unregisterHotkey(int id) {
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, id);
#endif
    m_registeredIds.removeAll(id);
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, long* result) {
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            int id = static_cast<int>(msg->wParam);
            if (m_registeredIds.contains(id)) {
                Q_EMIT hotkeyPressed(id);
                return true;
            }
        }
    }
#else
    Q_UNUSED(eventType) Q_UNUSED(message)
#endif
    return false;
}
