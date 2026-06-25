#pragma once

#ifdef _WIN32
#include <windows.h>
#include <cstring>

inline void viperDbg(const char* step) {
    HANDLE f = CreateFileW(L"C:\\Users\\Public\\viperrc_init.txt",
                           FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w = 0; WriteFile(f, step, (DWORD)strlen(step), &w, nullptr);
        CloseHandle(f);
    }
}
inline void viperPre(const char* step) {
    HANDLE f = CreateFileW(L"C:\\Users\\Public\\viperrc_pre.txt",
                           FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w = 0; WriteFile(f, step, (DWORD)strlen(step), &w, nullptr);
        CloseHandle(f);
    }
}
inline void viperMenu(const char* step) {
    HANDLE f = CreateFileW(L"C:\\Users\\Public\\viperrc_menu.txt",
                           FILE_APPEND_DATA, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD w = 0; WriteFile(f, step, (DWORD)strlen(step), &w, nullptr);
        CloseHandle(f);
    }
}

#else
inline void viperDbg(const char*) {}
inline void viperPre(const char*) {}
inline void viperMenu(const char*) {}
#endif
