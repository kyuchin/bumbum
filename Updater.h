#pragma once
#include <windows.h>

struct UpdateInfo {
    wchar_t tagName[64];
    wchar_t downloadUrl[512];
    wchar_t body[1024];
    bool available;
};

namespace Updater {
    void GetCurrentVersion(wchar_t* buf, int bufSize);
    bool CheckForUpdate(UpdateInfo& info);
    bool DownloadUpdate(const wchar_t* url, const wchar_t* savePath);
    bool ApplyUpdate(const wchar_t* newExePath);
    void CheckAndUpdate(HWND hwnd, bool silent);
}