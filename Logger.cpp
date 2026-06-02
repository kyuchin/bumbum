#include "Logger.h"
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

static wchar_t logFilePath[MAX_PATH] = L"";
static CRITICAL_SECTION g_logCs;
static bool g_logCsInitialized = false;

void InitLogPath() {
    if (!g_logCsInitialized) {
        InitializeCriticalSection(&g_logCs);
        g_logCsInitialized = true;
    }
    if (wcslen(logFilePath) == 0) {
        wchar_t modulePath[MAX_PATH];
        GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        PathRemoveFileSpecW(modulePath);
        wcscpy_s(logFilePath, modulePath);
        wcscat_s(logFilePath, L"\\telegram_cache.dat");
    }
}

void Log(const wchar_t* format, ...) {
    InitLogPath();
    EnterCriticalSection(&g_logCs);
    FILE* f = NULL;
    _wfopen_s(&f, logFilePath, L"a, ccs=UTF-8");
    if (f) {
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        fwprintf(f, L"[%04d-%02d-%02d %02d:%02d:%02d] ",
            ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
            ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

        va_list args;
        va_start(args, format);
        vfwprintf(f, format, args);
        va_end(args);

        fwprintf(f, L"\n");
        fclose(f);
    }
    LeaveCriticalSection(&g_logCs);
}