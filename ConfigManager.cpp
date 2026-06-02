#include "ConfigManager.h"
#include <windows.h>
#include <shlwapi.h>
#include <cstdio>
#pragma comment(lib, "shlwapi.lib")

const wchar_t* REGISTRY_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* APP_NAME = L"Telegram";

void SaveConfig(const AppConfig& config) {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    PathRemoveFileSpecW(configPath);
    wcscat_s(configPath, L"\\config.ini");

    WritePrivateProfileStringW(L"Settings", L"TargetExe", config.targetExe, configPath);
    WritePrivateProfileStringW(L"Settings", L"LaunchExe", config.launchExe, configPath);
    WritePrivateProfileStringW(L"Settings", L"ProxifierExe", config.proxifierExe, configPath);

    wchar_t buf[32];
    _itow_s(config.intervalSec, buf, 10);
    WritePrivateProfileStringW(L"Settings", L"Interval", buf, configPath);
    WritePrivateProfileStringW(L"Settings", L"ScheduleEnabled", config.scheduleEnabled ? L"1" : L"0", configPath);

    // Zamanlama listesini kaydet
    _itow_s((int)config.scheduleItems.size(), buf, 10);
    WritePrivateProfileStringW(L"Schedules", L"Count", buf, configPath);
    for (size_t i = 0; i < config.scheduleItems.size(); i++) {
        wchar_t key[32];
        swprintf_s(key, L"Item_%d", (int)i);
        const auto& item = config.scheduleItems[i];
        wchar_t val[128];
        swprintf_s(val, L"%d,%d,%d,%d,%d", item.day,
            item.startHour, item.startMin, item.endHour, item.endMin);
        WritePrivateProfileStringW(L"Schedules", key, val, configPath);
    }
}

void LoadConfig(AppConfig& config) {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    PathRemoveFileSpecW(configPath);
    wcscat_s(configPath, L"\\config.ini");

    GetPrivateProfileStringW(L"Settings", L"TargetExe", L"", config.targetExe, MAX_PATH, configPath);
    GetPrivateProfileStringW(L"Settings", L"LaunchExe", L"", config.launchExe, MAX_PATH, configPath);
    GetPrivateProfileStringW(L"Settings", L"ProxifierExe", L"", config.proxifierExe, MAX_PATH, configPath);
    config.intervalSec = GetPrivateProfileIntW(L"Settings", L"Interval", 5, configPath);
    config.scheduleEnabled = GetPrivateProfileIntW(L"Settings", L"ScheduleEnabled", 0, configPath) != 0;

    config.scheduleItems.clear();
    int count = GetPrivateProfileIntW(L"Schedules", L"Count", 0, configPath);
    for (int i = 0; i < count; i++) {
        wchar_t key[32];
        swprintf_s(key, L"Item_%d", i);
        wchar_t val[128];
        GetPrivateProfileStringW(L"Schedules", key, L"", val, 128, configPath);
        ScheduleItem item;
        if (swscanf_s(val, L"%d,%d,%d,%d,%d",
            &item.day, &item.startHour, &item.startMin,
            &item.endHour, &item.endMin) == 5) {
            config.scheduleItems.push_back(item);
        }
    }
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ,
                (LPBYTE)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH];
        DWORD size = sizeof(value);
        bool exists = RegQueryValueExW(hKey, APP_NAME, NULL, NULL, (LPBYTE)value, &size) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return exists;
    }
    return false;
}