#include "ProcessHelper.h"
#include <windows.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

bool IsProcessRunning(const wchar_t* exeName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    wchar_t exeNameOnly[MAX_PATH];
    wcscpy_s(exeNameOnly, exeName);
    PathStripPathW(exeNameOnly);
    bool found = false;
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, exeNameOnly) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return found;
}

void KillProcess(const wchar_t* exeName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    wchar_t exeNameOnly[MAX_PATH];
    wcscpy_s(exeNameOnly, exeName);
    PathStripPathW(exeNameOnly);
    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, exeNameOnly) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
}

bool CheckBanStatus() {
    return IsProcessRunning(L"cmd.exe");
}

bool CheckSoundFailedError() {
    HWND hwndError = FindWindowW(NULL, L"Error");
    if (!hwndError) return false;
    KillProcess(L"KnightOnLine.exe");
    PostMessage(hwndError, WM_CLOSE, 0, 0);
    return true;
}

// "Notice" veya "Bug Report" penceresini bulan yardımcı
void KillNoticeWindow() {
    HWND hwndNotice = FindWindowW(NULL, L"Notice");
    if (!hwndNotice)
        hwndNotice = FindWindowW(NULL, L"Bug Report");
    if (!hwndNotice) {
        // İçinde "Notice" geçen pencere ara
        hwndNotice = FindWindowExW(NULL, NULL, NULL, NULL);
        while (hwndNotice) {
            wchar_t title[128];
            GetWindowTextW(hwndNotice, title, 128);
            if (wcsstr(title, L"Notice") || wcsstr(title, L"Bug")) {
                break;
            }
            hwndNotice = FindWindowExW(NULL, hwndNotice, NULL, NULL);
        }
    }
    if (hwndNotice) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwndNotice, &pid);
        if (pid) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProcess) {
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
            }
        }
    }
}

void AutoHunterCheck() {
    KillProcess(L"xldr_KnightOnline_NA_loader_win32.exe");
    KillNoticeWindow();
    CheckSoundFailedError();
    HWND hwndTarget = FindWindowW(NULL, L"Proxy Server Login");
    if (hwndTarget) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwndTarget, &pid);
        if (pid) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProcess) {
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
            }
        }
    }
}