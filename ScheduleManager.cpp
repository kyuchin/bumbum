#include "ScheduleManager.h"
#include <ctime>

static std::vector<ScheduleItem> g_schedules;
static CRITICAL_SECTION g_cs;

void ScheduleManager::Initialize() {
    InitializeCriticalSection(&g_cs);
}

void ScheduleManager::Cleanup() {
    DeleteCriticalSection(&g_cs);
}

bool ScheduleManager::IsInSchedule() {
    EnterCriticalSection(&g_cs);
    std::vector<ScheduleItem> items = g_schedules;
    LeaveCriticalSection(&g_cs);

    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    int currentDay = (ltm.tm_wday + 6) % 7;   // Pazartesi = 0
    int currentMin = ltm.tm_hour * 60 + ltm.tm_min;

    for (const auto& item : items) {
        if (item.day != currentDay) continue;
        int startMin = item.startHour * 60 + item.startMin;
        int endMin   = item.endHour   * 60 + item.endMin;
        if (startMin <= endMin) {
            if (currentMin >= startMin && currentMin < endMin)
                return true;
        } else {
            // Gece yarısı geçişi
            if (currentMin >= startMin || currentMin < endMin)
                return true;
        }
    }
    return false;
}

void ScheduleManager::GetItems(std::vector<ScheduleItem>& outItems) {
    EnterCriticalSection(&g_cs);
    outItems = g_schedules;
    LeaveCriticalSection(&g_cs);
}

void ScheduleManager::SetItems(const std::vector<ScheduleItem>& items) {
    EnterCriticalSection(&g_cs);
    g_schedules = items;
    LeaveCriticalSection(&g_cs);
}

void ScheduleManager::AddItem(const ScheduleItem& item) {
    EnterCriticalSection(&g_cs);
    g_schedules.push_back(item);
    LeaveCriticalSection(&g_cs);
}

void ScheduleManager::RemoveItem(int index) {
    EnterCriticalSection(&g_cs);
    if (index >= 0 && index < (int)g_schedules.size())
        g_schedules.erase(g_schedules.begin() + index);
    LeaveCriticalSection(&g_cs);
}

void ScheduleManager::UpdateListbox(HWND hList) {
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    std::vector<ScheduleItem> items;
    GetItems(items);
    const wchar_t* days[] = {
        L"Pazartesi", L"Salı", L"Çarşamba", L"Perşembe",
        L"Cuma", L"Cumartesi", L"Pazar"
    };
    for (const auto& item : items) {
        wchar_t buf[100];
        swprintf_s(buf, L"%s: %02d:%02d - %02d:%02d",
            days[item.day], item.startHour, item.startMin,
            item.endHour, item.endMin);
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}