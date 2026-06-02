#pragma once
#include <windows.h>
#include "MainWindow.h"

#define ID_TIMER 107

DWORD WINAPI WatchdogThread(LPVOID lpParam);
void UpdateStatusLabels(AppContext* ctx);