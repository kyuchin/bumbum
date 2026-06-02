#include "MainWindow.h"
#include "ProcessHelper.h"
#include "ConfigManager.h"
#include "ScheduleManager.h"
#include "TrayManager.h"
#include "Watchdog.h"
#include "Updater.h"
#include "Logger.h"
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// Kontrol ID'leri
#define ID_TARGET_BROWSE 101
#define ID_LAUNCH_BROWSE 102
#define ID_START         103
#define ID_STOP          104
#define ID_INTERVAL      105
#define ID_AUTOSTART     106
#define ID_SCH_ENABLE    201
#define ID_SCH_DAY       202
#define ID_SCH_START_H   203
#define ID_SCH_START_M   204
#define ID_SCH_END_H     205
#define ID_SCH_END_M     206
#define ID_SCH_ADD       207
#define ID_SCH_DEL       208
#define ID_SCH_LIST      209
#define ID_SAVE          210
#define ID_RENAME_IE     211
#define ID_PROXY_BROWSE  212
#define ID_UPDATE_TIMER  213

// Yardımcı: Dosya seçme penceresi
void BrowseFile(wchar_t* buffer, HWND editBox, HWND hwndOwner) {
    OPENFILENAMEW ofn;
    wchar_t fileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = L"Exe Dosyaları (*.exe)\0*.exe\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"exe";
    if (GetOpenFileNameW(&ofn)) {
        wcscpy_s(buffer, MAX_PATH, fileName);
        SetWindowTextW(editBox, fileName);
    }
}

// Ayarları arayüze yükle
void ApplyLoadedConfigToUI(AppContext* ctx) {
    SetWindowTextW(ctx->hTargetEdit, ctx->config.targetExe);
    SetWindowTextW(ctx->hLaunchEdit, ctx->config.launchExe);
    SetWindowTextW(ctx->hProxifierEdit, ctx->config.proxifierExe);

    wchar_t buf[32];
    _itow_s(ctx->config.intervalSec, buf, 10);
    SetWindowTextW(ctx->hIntervalEdit, buf);

    SendMessageW(ctx->hSchEnableCheck, BM_SETCHECK,
        ctx->config.scheduleEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    ScheduleManager::SetItems(ctx->config.scheduleItems);
    ScheduleManager::UpdateListbox(ctx->hSchList);

    SendMessageW(ctx->hAutoStartCheck, BM_SETCHECK,
        IsAutoStartEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
}

// Başlatma
void StartWatchdog(HWND hwnd, AppContext* ctx) {
    wchar_t intervalStr[16];
    GetWindowTextW(ctx->hIntervalEdit, intervalStr, 16);
    ctx->config.intervalSec = _wtoi(intervalStr);
    if (ctx->config.intervalSec < 1) ctx->config.intervalSec = 1;

    // Güncel yolları al
    GetWindowTextW(ctx->hTargetEdit, ctx->config.targetExe, MAX_PATH);
    GetWindowTextW(ctx->hLaunchEdit, ctx->config.launchExe, MAX_PATH);
    GetWindowTextW(ctx->hProxifierEdit, ctx->config.proxifierExe, MAX_PATH);
    ctx->config.scheduleEnabled =
        SendMessageW(ctx->hSchEnableCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

    ctx->isRunning = true;
    ctx->checkInterval = ctx->config.intervalSec * 1000;
    ctx->countdownSeconds = ctx->config.intervalSec;
    ctx->hThread = CreateThread(NULL, 0, WatchdogThread, ctx, 0, NULL);
    SetTimer(hwnd, ID_TIMER, 500, NULL);

    EnableWindow(ctx->hStartBtn, FALSE);
    EnableWindow(ctx->hStopBtn, TRUE);
    SetWindowTextW(ctx->hStatusLabel, L"Durum: Çalışıyor...");
    TrayManager::UpdateTip(L"Telegram - Active");

    Log(L"Takip BAŞLATILDI");
    wchar_t msg[512];
    swprintf_s(msg, L"  Takip edilen: %s", ctx->config.targetExe);
    Log(msg);
    swprintf_s(msg, L"  Açılacak: %s", ctx->config.launchExe);
    Log(msg);
    if (wcslen(ctx->config.proxifierExe) > 0) {
        swprintf_s(msg, L"  Proxifier: %s (kill/relaunch)", ctx->config.proxifierExe);
        Log(msg);
    }
}

// Durdurma
void StopWatchdog(HWND hwnd, AppContext* ctx) {
    ctx->isRunning = false;
    if (ctx->hStopEvent) SetEvent(ctx->hStopEvent);
    KillTimer(hwnd, ID_TIMER);
    if (ctx->hThread) {
        WaitForSingleObject(ctx->hThread, 5000);
        CloseHandle(ctx->hThread);
        ctx->hThread = NULL;
    }
    if (ctx->hStopEvent) ResetEvent(ctx->hStopEvent);
    EnableWindow(ctx->hStartBtn, TRUE);
    EnableWindow(ctx->hStopBtn, FALSE);
    SetWindowTextW(ctx->hStatusLabel, L"Durum: Durduruldu");
    TrayManager::UpdateTip(L"Telegram - Idle");

    SetWindowTextW(ctx->hTargetStatusLabel, L"● Takip Edilen: -");
    SetWindowTextW(ctx->hLaunchStatusLabel, L"● Açılacak Exe: -");
    SetWindowTextW(ctx->hCountdownLabel, L"Sonraki kontrol: -");
    Log(L"Takip DURDURULDU");
}

// Pencere sınıfı kaydı
bool MainWindow::RegisterClass(HINSTANCE hInstance) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = MainWindow::WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"MSCTF_WindowClass";  // Windows native sınıf adı
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    return RegisterClassW(&wc) != 0;
}

// Pencere oluşturma
HWND MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    return CreateWindowW(L"MSCTF_WindowClass", L"Telegram",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 600,
        NULL, NULL, hInstance, NULL);
}

// Pencere mesaj işleyici
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppContext* ctx = (AppContext*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_CREATE) {
        ctx = new AppContext();
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ctx);

        // Kontrolleri oluştur
        CreateWindowW(L"STATIC", L"Takip Edilecek Exe:", WS_VISIBLE | WS_CHILD,
            10, 15, 130, 20, hwnd, NULL, NULL, NULL);
        ctx->hTargetEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
            140, 12, 280, 24, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Göz At", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            430, 10, 70, 28, hwnd, (HMENU)ID_TARGET_BROWSE, NULL, NULL);

        CreateWindowW(L"STATIC", L"Açılacak Exe:", WS_VISIBLE | WS_CHILD,
            10, 55, 130, 20, hwnd, NULL, NULL, NULL);
        ctx->hLaunchEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
            140, 52, 280, 24, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Göz At", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            430, 50, 70, 28, hwnd, (HMENU)ID_LAUNCH_BROWSE, NULL, NULL);

        CreateWindowW(L"STATIC", L"Proxifier Exe:", WS_VISIBLE | WS_CHILD,
            10, 95, 130, 20, hwnd, NULL, NULL, NULL);
        ctx->hProxifierEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
            140, 92, 280, 24, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Göz At", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            430, 90, 70, 28, hwnd, (HMENU)ID_PROXY_BROWSE, NULL, NULL);

        CreateWindowW(L"STATIC", L"Kontrol Aralığı (sn):", WS_VISIBLE | WS_CHILD,
            10, 135, 130, 20, hwnd, NULL, NULL, NULL);
        ctx->hIntervalEdit = CreateWindowW(L"EDIT", L"5", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            140, 132, 60, 24, hwnd, (HMENU)ID_INTERVAL, NULL, NULL);

        ctx->hSchEnableCheck = CreateWindowW(L"BUTTON", L"Planlı Duraklatma (Aktif)",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            10, 170, 200, 20, hwnd, (HMENU)ID_SCH_ENABLE, NULL, NULL);

        ctx->hSchDayCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 195, 100, 200, hwnd, (HMENU)ID_SCH_DAY, NULL, NULL);
        const wchar_t* days[] = { L"Pazartesi", L"Salı", L"Çarşamba", L"Perşembe",
                                  L"Cuma", L"Cumartesi", L"Pazar" };
        for (int i = 0; i < 7; i++)
            SendMessageW(ctx->hSchDayCombo, CB_ADDSTRING, 0, (LPARAM)days[i]);
        SendMessageW(ctx->hSchDayCombo, CB_SETCURSEL, 0, 0);

        ctx->hSchStartH = CreateWindowW(L"EDIT", L"13", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            120, 195, 30, 24, hwnd, (HMENU)ID_SCH_START_H, NULL, NULL);
        CreateWindowW(L"STATIC", L":", WS_VISIBLE | WS_CHILD, 152, 198, 10, 20, hwnd, NULL, NULL, NULL);
        ctx->hSchStartM = CreateWindowW(L"EDIT", L"55", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            160, 195, 30, 24, hwnd, (HMENU)ID_SCH_START_M, NULL, NULL);
        CreateWindowW(L"STATIC", L"-", WS_VISIBLE | WS_CHILD, 195, 198, 10, 20, hwnd, NULL, NULL, NULL);
        ctx->hSchEndH = CreateWindowW(L"EDIT", L"15", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            210, 195, 30, 24, hwnd, (HMENU)ID_SCH_END_H, NULL, NULL);
        CreateWindowW(L"STATIC", L":", WS_VISIBLE | WS_CHILD, 242, 198, 10, 20, hwnd, NULL, NULL, NULL);
        ctx->hSchEndM = CreateWindowW(L"EDIT", L"05", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
            250, 195, 30, 24, hwnd, (HMENU)ID_SCH_END_M, NULL, NULL);

        ctx->hSchAddBtn = CreateWindowW(L"BUTTON", L"Ekle", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            290, 193, 60, 28, hwnd, (HMENU)ID_SCH_ADD, NULL, NULL);

        ctx->hSchList = CreateWindowW(L"LISTBOX", NULL,
            WS_VISIBLE | WS_CHILD | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
            10, 230, 340, 80, hwnd, (HMENU)ID_SCH_LIST, NULL, NULL);
        ctx->hSchDelBtn = CreateWindowW(L"BUTTON", L"Seçili Olanı Sil",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            360, 230, 120, 28, hwnd, (HMENU)ID_SCH_DEL, NULL, NULL);

        CreateWindowW(L"BUTTON", L"Ayarları Kaydet", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            360, 265, 120, 28, hwnd, (HMENU)ID_SAVE, NULL, NULL);

        ctx->hStartBtn = CreateWindowW(L"BUTTON", L"Başlat", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            10, 320, 100, 35, hwnd, (HMENU)ID_START, NULL, NULL);
        ctx->hStopBtn = CreateWindowW(L"BUTTON", L"Durdur",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
            120, 320, 100, 35, hwnd, (HMENU)ID_STOP, NULL, NULL);

        ctx->hAutoStartCheck = CreateWindowW(L"BUTTON",
            L"Bilgisayar açılışında otomatik başlat",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            240, 325, 260, 25, hwnd, (HMENU)ID_AUTOSTART, NULL, NULL);

        CreateWindowW(L"BUTTON", L"IE Klasörünü Değiştir",
            WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            10, 355, 160, 28, hwnd, (HMENU)ID_RENAME_IE, NULL, NULL);

        ctx->hStatusLabel = CreateWindowW(L"STATIC", L"Durum: Bekliyor",
            WS_VISIBLE | WS_CHILD, 10, 395, 490, 20, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"STATIC", L"─────────────── Canlı Durum ───────────────",
            WS_VISIBLE | WS_CHILD, 10, 420, 490, 20, hwnd, NULL, NULL, NULL);
        ctx->hTargetStatusLabel = CreateWindowW(L"STATIC", L"● Takip Edilen: -",
            WS_VISIBLE | WS_CHILD, 10, 445, 490, 20, hwnd, NULL, NULL, NULL);
        ctx->hLaunchStatusLabel = CreateWindowW(L"STATIC", L"● Açılacak Exe: -",
            WS_VISIBLE | WS_CHILD, 10, 470, 490, 20, hwnd, NULL, NULL, NULL);
        ctx->hCountdownLabel = CreateWindowW(L"STATIC", L"Sonraki kontrol: -",
            WS_VISIBLE | WS_CHILD, 10, 495, 490, 20, hwnd, NULL, NULL, NULL);
        ctx->hBanStatusLabel = CreateWindowW(L"STATIC", L"✓ Ban yok",
            WS_VISIBLE | WS_CHILD, 10, 520, 490, 20, hwnd, NULL, NULL, NULL);

        // Konfigürasyonu yükle
        LoadConfig(ctx->config);
        ApplyLoadedConfigToUI(ctx);

        // Sistem tepsisi simgesini ekle
        TrayManager::Initialize(hwnd);

        // 6 saatte bir otomatik güncelleme kontrolü (21600000 ms)
        SetTimer(hwnd, ID_UPDATE_TIMER, 21600000, NULL);

        Log(L"Program başlatıldı");
        return 0;
    }

    // Diğer mesajlar
    switch (msg) {
        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case ID_TARGET_BROWSE:
                    BrowseFile(ctx->config.targetExe, ctx->hTargetEdit, hwnd);
                    SaveConfig(ctx->config);
                    break;
                case ID_LAUNCH_BROWSE:
                    BrowseFile(ctx->config.launchExe, ctx->hLaunchEdit, hwnd);
                    SaveConfig(ctx->config);
                    break;
                case ID_PROXY_BROWSE:
                    BrowseFile(ctx->config.proxifierExe, ctx->hProxifierEdit, hwnd);
                    SaveConfig(ctx->config);
                    break;
                case ID_AUTOSTART: {
                    bool checked = SendMessageW(ctx->hAutoStartCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    SetAutoStart(checked);
                    break;
                }
                case ID_SCH_ADD: {
                    wchar_t buf[8];
                    int sH, sM, eH, eM;
                    GetWindowTextW(ctx->hSchStartH, buf, 8); sH = _wtoi(buf);
                    GetWindowTextW(ctx->hSchStartM, buf, 8); sM = _wtoi(buf);
                    GetWindowTextW(ctx->hSchEndH, buf, 8);   eH = _wtoi(buf);
                    GetWindowTextW(ctx->hSchEndM, buf, 8);   eM = _wtoi(buf);
                    int day = (int)SendMessageW(ctx->hSchDayCombo, CB_GETCURSEL, 0, 0);
                    if (day == CB_ERR) day = 0;

                    ScheduleItem item = { day, sH, sM, eH, eM };
                    ScheduleManager::AddItem(item);
                    ScheduleManager::UpdateListbox(ctx->hSchList);

                    // Konfigürasyona da ekle (senkron)
                    ctx->config.scheduleItems.clear();
                    ScheduleManager::GetItems(ctx->config.scheduleItems);
                    SaveConfig(ctx->config);
                    break;
                }
                case ID_SCH_DEL: {
                    int sel = (int)SendMessageW(ctx->hSchList, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR) {
                        ScheduleManager::RemoveItem(sel);
                        ScheduleManager::UpdateListbox(ctx->hSchList);
                        ctx->config.scheduleItems.clear();
                        ScheduleManager::GetItems(ctx->config.scheduleItems);
                        SaveConfig(ctx->config);
                    }
                    break;
                }
                case ID_SAVE: {
                    // Checkbox durumlarını config'e oku
                    ctx->config.scheduleEnabled =
                        SendMessageW(ctx->hSchEnableCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    GetWindowTextW(ctx->hTargetEdit, ctx->config.targetExe, MAX_PATH);
                    GetWindowTextW(ctx->hLaunchEdit, ctx->config.launchExe, MAX_PATH);
                    GetWindowTextW(ctx->hProxifierEdit, ctx->config.proxifierExe, MAX_PATH);
                    wchar_t buf[16];
                    GetWindowTextW(ctx->hIntervalEdit, buf, 16);
                    ctx->config.intervalSec = _wtoi(buf);
                    if (ctx->config.intervalSec < 1) ctx->config.intervalSec = 1;

                    SaveConfig(ctx->config);
                    MessageBoxW(hwnd, L"Ayarlar kaydedildi!", L"Bilgi", MB_ICONINFORMATION);
                    Log(L"Ayarlar manuel olarak kaydedildi");
                    break;
                }
                case ID_START: {
                    if (wcslen(ctx->config.targetExe) == 0 || wcslen(ctx->config.launchExe) == 0) {
                        MessageBoxW(hwnd, L"Lütfen her iki exe dosyasını da seçin!", L"Uyarı", MB_ICONWARNING);
                        break;
                    }
                    StartWatchdog(hwnd, ctx);
                    break;
                }
                case ID_STOP:
                    StopWatchdog(hwnd, ctx);
                    break;
                case ID_RENAME_IE: {
                    const wchar_t* oldPath = L"C:\\Program Files\\Internet Explorer";
                    const wchar_t* newPath = L"C:\\Program Files\\1Internet Explorer";
                    if (MoveFileW(oldPath, newPath)) {
                        MessageBoxW(hwnd, L"Klasör başarıyla yeniden adlandırıldı:\nC:\\Program Files\\1Internet Explorer",
                            L"Bilgi", MB_ICONINFORMATION);
                        Log(L"IE klasörü yeniden adlandırıldı");
                    } else {
                        DWORD err = GetLastError();
                        wchar_t msg[256];
                        swprintf_s(msg, L"Klasör yeniden adlandırılamadı!\nHata kodu: %lu\n\nYönetici olarak çalıştırmayı deneyin.", err);
                        MessageBoxW(hwnd, msg, L"Hata", MB_ICONERROR);
                        wchar_t logMsg[256];
                        swprintf_s(logMsg, L"IE klasörü yeniden adlandırma BAŞARISIZ - Hata: %lu", err);
                        Log(logMsg);
                    }
                    break;
                }
                case ID_TRAY_SHOW:
                    TrayManager::RestoreFromTray(hwnd);
                    break;
                case ID_TRAY_START:
                    SendMessageW(hwnd, WM_COMMAND, ID_START, 0);
                    break;
                case ID_TRAY_STOP:
                    SendMessageW(hwnd, WM_COMMAND, ID_STOP, 0);
                    break;
                case ID_TRAY_UPDATE:
                    Updater::CheckAndUpdate(hwnd, false);
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            break;
        }
        case WM_TIMER:
            if (wParam == ID_TIMER && ctx) UpdateStatusLabels(ctx);
            if (wParam == ID_UPDATE_TIMER) Updater::CheckAndUpdate(hwnd, true);
            break;
        case WM_USER + 1:
            if (ctx) UpdateStatusLabels(ctx);
            break;
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) TrayManager::MinimizeToTray(hwnd);
            break;
        case WM_CLOSE:
            TrayManager::MinimizeToTray(hwnd);
            return 0;
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP)
                TrayManager::ShowMenu(hwnd);
            else if (lParam == WM_LBUTTONDBLCLK)
                TrayManager::RestoreFromTray(hwnd);
            break;
        case WM_DESTROY:
            Log(L"Program kapatılıyor");
            KillTimer(hwnd, ID_UPDATE_TIMER);
            StopWatchdog(hwnd, ctx);
            SaveConfig(ctx->config);
            TrayManager::Cleanup();
            delete ctx;
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}