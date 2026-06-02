#pragma once
#include <windows.h>

bool IsProcessRunning(const wchar_t* exeName);
void KillProcess(const wchar_t* exeName);
bool CheckBanStatus();
bool CheckSoundFailedError();
void AutoHunterCheck();
void KillNoticeWindow();