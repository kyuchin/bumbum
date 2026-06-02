#include "Watchdog.h"
#include "ProcessHelper.h"
#include "ScheduleManager.h"
#include "Logger.h"
#include <shellapi.h>
#include <shlwapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

void LaunchProcess(const wchar_t* exePath) {
    wchar_t workingDir[MAX_PATH];
    wcscpy_s(workingDir, exePath);
    PathRemoveFileSpecW(workingDir);
    ShellExecuteW(NULL, L"open", exePath, NULL, workingDir, SW_SHOWNORMAL);
}

DWORD WINAPI WatchdogThread(LPVOID lpParam) {
    AppContext* ctx = (AppContext*)lpParam;
    const DWORD cooldown = 60000;
    bool banLogged = false;
    HANDLE hStop = ctx->hStopEvent;

    while (ctx->isRunning) {
        AutoHunterCheck();

        bool inSchedule = ctx->config.scheduleEnabled ? ScheduleManager::IsInSchedule() : false;

        if (inSchedule) {
            PostMessage(GetParent(ctx->hStatusLabel), WM_USER + 1, 0, 0);
            if (wcslen(ctx->config.targetExe) > 0 && IsProcessRunning(ctx->config.targetExe))
                KillProcess(ctx->config.targetExe);
            ctx->countdownSeconds = 0;
            if (WaitForSingleObject(hStop, 1000) == WAIT_OBJECT_0) break;
            continue;
        }

        ctx->countdownSeconds = ctx->config.intervalSec;

        // Ban kontrolü ve launch
        if (wcslen(ctx->config.targetExe) > 0 && wcslen(ctx->config.launchExe) > 0) {
            if (!IsProcessRunning(ctx->config.targetExe)) {
                if (CheckBanStatus()) {
                    if (!banLogged) {
                        Log(L"BAN ALGILANDI - CMD.exe açık, bekleniyor...");
                        banLogged = true;
                    }
                    PostMessage(GetParent(ctx->hStatusLabel), WM_USER + 1, 0, 0);
                } else {
                    if (banLogged) {
                        Log(L"Ban süresi bitti");
                        banLogged = false;
                    }
                    DWORD now = GetTickCount();
                    DWORD elapsed = now - ctx->lastLaunchTime;
                    if (ctx->lastLaunchTime == 0 || elapsed >= cooldown) {
                        ctx->isLaunching = true;
                        PostMessage(GetParent(ctx->hStatusLabel), WM_USER + 1, 0, 0);

                        // Proxifier'ı kapat (varsa)
                        if (wcslen(ctx->config.proxifierExe) > 0 && IsProcessRunning(ctx->config.proxifierExe)) {
                            wchar_t logBuf[512];
                            swprintf_s(logBuf, L"Proxifier kapatılıyor: %s", ctx->config.proxifierExe);
                            Log(logBuf);
                            KillProcess(ctx->config.proxifierExe);
                            if (WaitForSingleObject(hStop, 2000) == WAIT_OBJECT_0) break;
                        }

                        // Proxifier'ı tekrar aç (varsa) - proxy bağlantısı kurulsun
                        if (wcslen(ctx->config.proxifierExe) > 0) {
                            wchar_t logBuf[512];
                            swprintf_s(logBuf, L"Proxifier yeniden başlatılıyor: %s", ctx->config.proxifierExe);
                            Log(logBuf);
                            LaunchProcess(ctx->config.proxifierExe);
                            if (WaitForSingleObject(hStop, 2000) == WAIT_OBJECT_0) break;
                        }

                        wchar_t launchBuf[512];
                        swprintf_s(launchBuf, L"Açılacak exe başlatılıyor: %s", ctx->config.launchExe);
                        Log(launchBuf);
                        if (IsProcessRunning(ctx->config.launchExe)) {
                            swprintf_s(launchBuf, L"Mevcut launcher kapatılıyor: %s", ctx->config.launchExe);
                            Log(launchBuf);
                            KillProcess(ctx->config.launchExe);
                            if (WaitForSingleObject(hStop, 1500) == WAIT_OBJECT_0) break;
                        }
                        LaunchProcess(ctx->config.launchExe);
                        ctx->lastLaunchTime = GetTickCount();
                        if (WaitForSingleObject(hStop, 2000) == WAIT_OBJECT_0) break;

                        ctx->isLaunching = false;
                    }
                }
            } else {
                ctx->lastLaunchTime = 0;
            }
        }

        // Geri sayım
        bool stopped = false;
        for (int i = ctx->checkInterval / 1000; i > 0 && ctx->isRunning; i--) {
            ctx->countdownSeconds = i;
            if (WaitForSingleObject(hStop, 1000) == WAIT_OBJECT_0) {
                stopped = true;
                break;
            }
        }
        if (stopped) break;
    }
    ctx->isLaunching = false;
    return 0;
}

void UpdateStatusLabels(AppContext* ctx) {
    if (!ctx->isRunning) return;

    wchar_t targetName[MAX_PATH];
    wcscpy_s(targetName, ctx->config.targetExe);
    PathStripPathW(targetName);
    wchar_t launchName[MAX_PATH];
    wcscpy_s(launchName, ctx->config.launchExe);
    PathStripPathW(launchName);

    bool banActive = CheckBanStatus();
    SetWindowTextW(ctx->hBanStatusLabel, banActive ? L"⛔ BAN AKTİF - CMD açık" : L"✓ Ban yok");

    if (wcslen(ctx->config.targetExe) > 0) {
        wchar_t buf[256];
        swprintf_s(buf, IsProcessRunning(ctx->config.targetExe) ? L"● %s: AKTİF" : L"○ %s: KAPALI", targetName);
        SetWindowTextW(ctx->hTargetStatusLabel, buf);
    }
    if (wcslen(ctx->config.launchExe) > 0) {
        wchar_t buf[256];
        if (ctx->isLaunching)
            swprintf_s(buf, L"► %s: AÇILIYOR...", launchName);
        else if (IsProcessRunning(ctx->config.launchExe))
            swprintf_s(buf, L"● %s: AKTİF", launchName);
        else
            swprintf_s(buf, L"○ %s: KAPALI", launchName);
        SetWindowTextW(ctx->hLaunchStatusLabel, buf);
    }
    wchar_t buf[64];
    swprintf_s(buf, L"Sonraki kontrol: %d saniye", ctx->countdownSeconds.load());
    SetWindowTextW(ctx->hCountdownLabel, buf);
}