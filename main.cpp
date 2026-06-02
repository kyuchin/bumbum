#include <windows.h>
#include "MainWindow.h"
#include "ScheduleManager.h"
#include "Logger.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\{6B3F5A32-4A0C-4B44-8F9C-A1B2C3D4E5F6}");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    ScheduleManager::Initialize();
    InitLogPath();

    if (!MainWindow::RegisterClass(hInstance)) { CloseHandle(hMutex); return 1; }

    HWND hwnd = MainWindow::Create(hInstance, nCmdShow);
    if (!hwnd) { CloseHandle(hMutex); return 1; }

    if (wcsstr(GetCommandLineW(), L"-hide")) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, nCmdShow);
    }
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ScheduleManager::Cleanup();
    CloseHandle(hMutex);
    return (int)msg.wParam;
}