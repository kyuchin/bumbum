#include "Updater.h"
#include "Logger.h"
#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <cstdio>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

static const wchar_t* GITHUB_API_HOST = L"api.github.com";
static const wchar_t* GITHUB_API_PATH = L"/repos/kyuchin/bumbum/releases/latest";

static int ParseVersion(const wchar_t* ver) {
    int major = 0, minor = 0, patch = 0, build = 0;
    swscanf_s(ver, L"%d.%d.%d.%d", &major, &minor, &patch, &build);
    return (major * 1000000) + (minor * 10000) + (patch * 100) + build;
}

static bool JsonFind(const wchar_t* json, const wchar_t* key, wchar_t* out, int outSize) {
    wchar_t search[128];
    swprintf_s(search, L"\"%s\"", key);
    const wchar_t* pos = wcsstr(json, search);
    if (!pos) return false;
    pos = wcsstr(pos, L":");
    if (!pos) return false;
    pos++;
    while (*pos == L' ' || *pos == L'\t') pos++;
    if (*pos == L'"') {
        pos++;
        const wchar_t* end = wcsstr(pos, L"\"");
        if (!end) return false;
        int len = (int)(end - pos);
        if (len >= outSize) len = outSize - 1;
        wcsncpy_s(out, outSize, pos, len);
        out[len] = L'\0';
        return true;
    }
    return false;
}

void Updater::GetCurrentVersion(wchar_t* buf, int bufSize) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(exePath, &handle);
    if (size == 0) {
        wcscpy_s(buf, bufSize, L"0.0.0.0");
        return;
    }

    void* data = malloc(size);
    if (!GetFileVersionInfoW(exePath, handle, size, data)) {
        free(data);
        wcscpy_s(buf, bufSize, L"0.0.0.0");
        return;
    }

    VS_FIXEDFILEINFO* fileInfo = NULL;
    UINT fileInfoSize = 0;
    if (VerQueryValueW(data, L"\\", (void**)&fileInfo, &fileInfoSize) && fileInfo) {
        int major = HIWORD(fileInfo->dwFileVersionMS);
        int minor = LOWORD(fileInfo->dwFileVersionMS);
        int patch = HIWORD(fileInfo->dwFileVersionLS);
        int build = LOWORD(fileInfo->dwFileVersionLS);
        swprintf_s(buf, bufSize, L"%d.%d.%d.%d", major, minor, patch, build);
    } else {
        wcscpy_s(buf, bufSize, L"0.0.0.0");
    }
    free(data);
}

static bool HttpGet(const wchar_t* host, const wchar_t* path, wchar_t* response, int responseSize, const wchar_t* token = NULL) {
    HINTERNET hSession = WinHttpOpen(L"TelegramUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    wchar_t headers[1024];
    if (token && wcslen(token) > 0) {
        swprintf_s(headers, L"User-Agent: TelegramUpdater\r\nAccept: application/vnd.github.v3+json\r\nAuthorization: token %s", token);
    } else {
        wcscpy_s(headers, L"User-Agent: TelegramUpdater\r\nAccept: application/vnd.github.v3+json");
    }
    WinHttpAddRequestHeaders(hRequest, headers, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    bool success = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        DWORD bytesRead = 0;
        char buffer[8192] = {0};
        DWORD totalRead = 0;

        while (WinHttpReadData(hRequest, buffer + totalRead,
                sizeof(buffer) - totalRead - 1, &bytesRead) && bytesRead > 0) {
            totalRead += bytesRead;
            bytesRead = 0;
        }

        if (totalRead > 0) {
            buffer[totalRead] = '\0';
            int converted = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, response, responseSize);
            success = (converted > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

bool Updater::CheckForUpdate(UpdateInfo& info) {
    info.available = false;
    ZeroMemory(info.tagName, sizeof(info.tagName));
    ZeroMemory(info.downloadUrl, sizeof(info.downloadUrl));
    ZeroMemory(info.body, sizeof(info.body));

    wchar_t token[256] = {0};
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    PathRemoveFileSpecW(configPath);
    wcscat_s(configPath, L"\\config.ini");
    GetPrivateProfileStringW(L"Settings", L"GitHubToken", L"", token, 256, configPath);

    wchar_t json[8192] = {0};
    if (!HttpGet(GITHUB_API_HOST, GITHUB_API_PATH, json, 8192, token)) {
        Log(L"Update check failed: HTTP request error");
        return false;
    }

    if (!JsonFind(json, L"tag_name", info.tagName, 64)) {
        Log(L"Update check failed: tag_name not found");
        return false;
    }

    wchar_t currentVer[64];
    GetCurrentVersion(currentVer, 64);

    int currentNum = ParseVersion(currentVer);
    int remoteNum = ParseVersion(info.tagName);

    if (remoteNum > currentNum) {
        const wchar_t* assetsPos = wcsstr(json, L"\"assets\"");
        if (assetsPos) {
            const wchar_t* urlPos = wcsstr(assetsPos, L"\"browser_download_url\"");
            if (urlPos) {
                JsonFind(urlPos, L"browser_download_url", info.downloadUrl, 512);
            }
        }

        if (wcslen(info.downloadUrl) == 0) {
            Log(L"Update check failed: download URL not found");
            return false;
        }

        info.available = true;
        wchar_t logBuf[512];
        swprintf_s(logBuf, L"Update available: %s -> %s", currentVer, info.tagName);
        Log(logBuf);
    } else {
        Log(L"No update available");
    }

    return true;
}

bool Updater::DownloadUpdate(const wchar_t* url, const wchar_t* savePath) {
    wchar_t host[256] = {0};
    wchar_t path[512] = {0};

    const wchar_t* protoEnd = wcsstr(url, L"://");
    const wchar_t* hostStart = protoEnd ? protoEnd + 3 : url;
    const wchar_t* pathStart = wcsstr(hostStart, L"/");

    if (pathStart) {
        int hostLen = (int)(pathStart - hostStart);
        wcsncpy_s(host, 256, hostStart, hostLen);
        wcscpy_s(path, 512, pathStart);
    } else {
        wcscpy_s(host, 256, hostStart);
        wcscpy_s(path, 512, L"/");
    }

    HINTERNET hSession = WinHttpOpen(L"TelegramUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool success = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        FILE* f = NULL;
        _wfopen_s(&f, savePath, L"wb");
        if (f) {
            DWORD bytesRead = 0;
            char buffer[4096];
            while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                fwrite(buffer, 1, bytesRead, f);
                bytesRead = 0;
            }
            fclose(f);
            success = true;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

bool Updater::ApplyUpdate(const wchar_t* newExePath) {
    wchar_t currentExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    wchar_t batPath[MAX_PATH];
    wcscpy_s(batPath, currentExe);
    PathRemoveFileSpecW(batPath);
    wcscat_s(batPath, L"\\update.bat");

    FILE* f = NULL;
    _wfopen_s(&f, batPath, L"w");
    if (!f) return false;

    fwprintf(f, L"@echo off\n");
    fwprintf(f, L"timeout /t 2 /nobreak >nul\n");
    fwprintf(f, L"del \"%s\"\n", currentExe);
    fwprintf(f, L"move /y \"%s\" \"%s\"\n", newExePath, currentExe);
    fwprintf(f, L"start \"\" \"%s\"\n", currentExe);
    fwprintf(f, L"del \"%%~f0\"\n");
    fclose(f);

    ShellExecuteW(NULL, L"open", batPath, NULL, NULL, SW_HIDE);
    return true;
}

void Updater::CheckAndUpdate(HWND hwnd, bool silent) {
    UpdateInfo info;
    if (!CheckForUpdate(info)) {
        if (!silent)
            MessageBoxW(hwnd, L"Güncelleme kontrolü yapılamadı.\nGitHub bağlantısını kontrol edin.",
                L"Güncelleme", MB_ICONWARNING);
        return;
    }

    if (!info.available) {
        if (!silent)
            MessageBoxW(hwnd, L"Program güncel! Yeni sürüm bulunamadı.",
                L"Güncelleme", MB_ICONINFORMATION);
        return;
    }

    wchar_t msg[1024];
    swprintf_s(msg, L"Yeni sürüm mevcut: %s\n\nGüncellemek ister misiniz?",
        info.tagName);

    int result = MessageBoxW(hwnd, msg, L"Güncelleme Mevcut",
        MB_YESNO | MB_ICONQUESTION);

    if (result != IDYES) return;

    wchar_t tempPath[MAX_PATH];
    GetModuleFileNameW(NULL, tempPath, MAX_PATH);
    PathRemoveFileSpecW(tempPath);
    wcscat_s(tempPath, L"\\Telegram.exe.new");

    Log(L"Downloading update...");
    if (!DownloadUpdate(info.downloadUrl, tempPath)) {
        MessageBoxW(hwnd, L"İndirme başarısız!", L"Hata", MB_ICONERROR);
        return;
    }

    Log(L"Update downloaded, applying...");
    if (ApplyUpdate(tempPath)) {
        Log(L"Update applied, restarting...");
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    } else {
        MessageBoxW(hwnd, L"Güncelleme uygulanamadı!", L"Hata", MB_ICONERROR);
    }
}
