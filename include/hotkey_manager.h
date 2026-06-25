#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>

// Windows global hotkey via RegisterHotKey WinAPI
class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    // Register Ctrl+Shift+R by default; call before QApplication event loop
    bool registerHotkey(int id, Qt::Key key, Qt::KeyboardModifiers mods);
    void unregisterHotkey(int id);

Q_SIGNALS:
    void hotkeyPressed(int id);

protected:
    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result) override;

private:
    QList<int> m_registeredIds;
};
