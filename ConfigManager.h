#pragma once
#include <windows.h>
#include <vector>
#include "ScheduleManager.h"   // ScheduleItem tanımı için

struct AppConfig {
    wchar_t targetExe[MAX_PATH];
    wchar_t launchExe[MAX_PATH];
    wchar_t proxifierExe[MAX_PATH];
    int intervalSec;
    bool scheduleEnabled;
    std::vector<ScheduleItem> scheduleItems;

    AppConfig() : intervalSec(5), scheduleEnabled(false) {
        ZeroMemory(targetExe, sizeof(targetExe));
        ZeroMemory(launchExe, sizeof(launchExe));
        ZeroMemory(proxifierExe, sizeof(proxifierExe));
    }
};

void SaveConfig(const AppConfig& config);
void LoadConfig(AppConfig& config);
void SetAutoStart(bool enable);
bool IsAutoStartEnabled();