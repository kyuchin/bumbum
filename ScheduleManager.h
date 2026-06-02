#pragma once
#include <windows.h>
#include <vector>

struct ScheduleItem {
    int day;            // 0=Pazartesi, 6=Pazar
    int startHour, startMin;
    int endHour, endMin;
};

namespace ScheduleManager {
    void Initialize();
    void Cleanup();
    bool IsInSchedule();                           // O an saat planı içinde mi?
    void GetItems(std::vector<ScheduleItem>& outItems);
    void SetItems(const std::vector<ScheduleItem>& items);
    void AddItem(const ScheduleItem& item);
    void RemoveItem(int index);
    void UpdateListbox(HWND hList);                // Listbox'ı tazele
}