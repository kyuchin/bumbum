#pragma once
#include <windows.h>
#include <atomic>
#include "ConfigManager.h"   // AppConfig yapısı

// Pencereye ait tüm verileri taşıyan yapı
struct AppContext {
    // UI kontrolleri
    HWND hTargetEdit, hLaunchEdit, hProxifierEdit, hIntervalEdit;
    HWND hStartBtn, hStopBtn;
    HWND hStatusLabel;
    HWND hAutoStartCheck;
    HWND hTargetStatusLabel, hLaunchStatusLabel, hCountdownLabel, hBanStatusLabel;
    HWND hSchEnableCheck, hSchDayCombo, hSchStartH, hSchStartM, hSchEndH, hSchEndM;
    HWND hSchList, hSchAddBtn, hSchDelBtn;

    // Ayarlar
    AppConfig config;

    // Çalışma anı verileri
    std::atomic<bool> isRunning;
    std::atomic<bool> isLaunching;
    DWORD lastLaunchTime;
    std::atomic<int> countdownSeconds;
    int checkInterval;   // milisaniye cinsinden kontrol aralığı
    HANDLE hThread;
    HANDLE hStopEvent;

    AppContext() : isRunning(false), isLaunching(false),
                   lastLaunchTime(0), countdownSeconds(0), checkInterval(5000),
                   hThread(NULL), hStopEvent(NULL) {
        hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    }
    ~AppContext() {
        if (hStopEvent) CloseHandle(hStopEvent);
    }
};

namespace MainWindow {
    bool RegisterClass(HINSTANCE hInstance);
    HWND Create(HINSTANCE hInstance, int nCmdShow);
    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}