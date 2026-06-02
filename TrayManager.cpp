#include "TrayManager.h"
#include "Logger.h"

static NOTIFYICONDATAW nid = {0};
static HWND g_hWnd = NULL;
static bool g_minimized = false;

void TrayManager::Initialize(HWND hwnd) {
    g_hWnd = hwnd;
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_WINLOGO);
    wcscpy_s(nid.szTip, L"Telegram");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayManager::Cleanup() {
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayManager::UpdateTip(const wchar_t* tip) {
    wcscpy_s(nid.szTip, tip);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayManager::ShowMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, L"Göster");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_START, L"Başlat");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_STOP, L"Durdur");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_UPDATE, L"Güncellemeleri Kontrol Et");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Çıkış");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
        pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void TrayManager::MinimizeToTray(HWND hwnd) {
    ShowWindow(hwnd, SW_HIDE);
    g_minimized = true;
    Log(L"Program tepsiye küçültüldü");
}

void TrayManager::RestoreFromTray(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    g_minimized = false;
    Log(L"Program tepsiden geri yüklendi");
}

bool TrayManager::IsMinimized() {
    return g_minimized;
}