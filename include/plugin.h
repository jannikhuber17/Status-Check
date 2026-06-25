#pragma once
#include <QString>

namespace Plugin {
    void    openHotkeyDialog(const char* keyword);
    QString getCurrentHotkey(const char* keyword);
}
