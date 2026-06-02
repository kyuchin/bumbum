#pragma once
#include <windows.h>

#define WM_TRAYICON   (WM_USER + 100)
#define ID_TRAY_SHOW  301
#define ID_TRAY_START 302
#define ID_TRAY_STOP  303
#define ID_TRAY_EXIT  304
#define ID_TRAY_UPDATE 305

namespace TrayManager {
    void Initialize(HWND hwnd);
    void Cleanup();
    void UpdateTip(const wchar_t* tip);
    void ShowMenu(HWND hwnd);
    void MinimizeToTray(HWND hwnd);
    void RestoreFromTray(HWND hwnd);
    bool IsMinimized();
}