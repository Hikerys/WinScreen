#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <urlmon.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <dshow.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <deque>

using std::min;
using std::max;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

// Константы приложения
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_FULLSCREEN     2001
#define ID_TRAY_LIVE_AREA      2002
#define ID_TRAY_FROZEN_AREA    2003
#define ID_TRAY_OPEN_FOLDER    2004
#define ID_TRAY_SETTINGS       2005
#define ID_TRAY_AUTOSTART      2006
#define ID_TRAY_EXIT           2007
#define ID_TRAY_RECORD_VIDEO   2008
#define ID_TRAY_OPEN_VIDEO_DIR 2009

#define REG_RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define REG_APP_NAME L"WinScreen"
#define REG_KEYBOARD_KEY L"Control Panel\\Keyboard"
#define REG_SNIP_KEY L"PrintScreenShortcutManagedBySnippingToolEnabled"

// Конфигурация горячей клавиши
struct HotkeyConfig {
    DWORD vkCode;
    bool ctrl;
    bool shift;
    bool alt;
    bool win;
    std::wstring displayText;
};

// Глобальные переменные состояния
HINSTANCE g_hInstance = NULL;
HWND g_hMainWnd = NULL;
HWND g_hSettingsWnd = NULL;
NOTIFYICONDATAW g_nid = { sizeof(NOTIFYICONDATAW) };
HHOOK g_hKeyboardHook = NULL;
ULONG_PTR g_gdiplusToken = 0;
std::wstring g_saveDir;
std::wstring g_videoSaveDir;
std::wstring g_fileFormat = L"png";
int g_jpgQuality = 90; // По умолчанию 90% (диапазон 1-100%)
bool g_copyClipboard = true;
bool g_saveDisk = true;
bool g_showNotifications = true;
bool g_isRecording = false;
PROCESS_INFORMATION g_ffmpegProc = { 0 };
HANDLE g_hFFmpegStdin = NULL;
bool g_recordSysAudio = false;
bool g_recordMic = false;
std::wstring g_selectedMic = L"";

// Горячие клавиши по умолчанию
HotkeyConfig g_hkFullscreen = { VK_SNAPSHOT, true,  false, false, false, L"Ctrl + PrintScreen" };
HotkeyConfig g_hkLive       = { VK_SNAPSHOT, false, true,  false, false, L"Shift + PrintScreen" };
HotkeyConfig g_hkFrozen     = { VK_SNAPSHOT, false, false, false, false, L"PrintScreen" };
HotkeyConfig g_hkRecord     = { VK_SNAPSHOT, true,  true,  false, false, L"Ctrl + Shift + PrintScreen" };

// Переменные для интерактивной записи горячих клавиш в диалоге
HotkeyConfig g_tempHkFull;
HotkeyConfig g_tempHkLive;
HotkeyConfig g_tempHkFrozen;
HotkeyConfig g_tempHkRecord;
int g_recordingTarget = 0; // 0 = нет, 1 = полный экран, 2 = живое выделение, 3 = заморозка, 4 = видеозапись

static bool s_recCtrl  = false;
static bool s_recShift = false;
static bool s_recAlt   = false;
static bool s_recWin   = false;

// Элементы управления строк горячих клавиш и директорий
static HWND hEditDir = NULL;
static HWND hEditVideoDir = NULL;
static HWND hEditHk1 = NULL, hBtnHk1 = NULL;
static HWND hEditHk2 = NULL, hBtnHk2 = NULL;
static HWND hEditHk3 = NULL, hBtnHk3 = NULL;
static HWND hEditHk4 = NULL, hBtnHk4 = NULL;

// Функция форматирования комбинации клавиш в читаемый русский текст
std::wstring FormatHotkey(DWORD vk, bool ctrl, bool shift, bool alt, bool win = false) {
    std::wstring res;
    if (win) res += L"Win + ";
    if (ctrl) res += L"Ctrl + ";
    if (shift) res += L"Shift + ";
    if (alt) res += L"Alt + ";

    if (vk == VK_SNAPSHOT) {
        res += L"PrintScreen";
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        res += L"F" + std::to_wstring(vk - VK_F1 + 1);
    } else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        res += (WCHAR)vk;
    } else if (vk == VK_SPACE) {
        res += L"Space";
    } else if (vk == VK_RETURN) {
        res += L"Enter";
    } else if (vk == VK_TAB) {
        res += L"Tab";
    } else if (vk == VK_BACK) {
        res += L"Backspace";
    } else if (vk == VK_INSERT) {
        res += L"Insert";
    } else if (vk == VK_DELETE) {
        res += L"Delete";
    } else if (vk == VK_HOME) {
        res += L"Home";
    } else if (vk == VK_END) {
        res += L"End";
    } else if (vk == VK_PRIOR) {
        res += L"PageUp";
    } else if (vk == VK_NEXT) {
        res += L"PageDown";
    } else if (vk == VK_LEFT) {
        res += L"Left";
    } else if (vk == VK_RIGHT) {
        res += L"Right";
    } else if (vk == VK_UP) {
        res += L"Up";
    } else if (vk == VK_DOWN) {
        res += L"Down";
    } else if (vk == VK_OEM_MINUS) {
        res += L"-";
    } else if (vk == VK_OEM_PLUS) {
        res += L"+";
    } else if (vk == VK_OEM_1) {
        res += L";";
    } else if (vk == VK_OEM_2) {
        res += L"/";
    } else if (vk == VK_OEM_3) {
        res += L"`";
    } else if (vk == VK_OEM_4) {
        res += L"[";
    } else if (vk == VK_OEM_5) {
        res += L"\\";
    } else if (vk == VK_OEM_6) {
        res += L"]";
    } else if (vk == VK_OEM_7) {
        res += L"'";
    } else if (vk == VK_OEM_COMMA) {
        res += L",";
    } else if (vk == VK_OEM_PERIOD) {
        res += L".";
    } else {
        WCHAR name[64] = { 0 };
        UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        if (GetKeyNameTextW(scanCode << 16, name, 64) > 0) {
            res += name;
        } else {
            res += L"Key_" + std::to_wstring(vk);
        }
    }
    return res;
}

// Структура для выделения области
struct SelectionState {
    bool isSelecting = false;
    POINT startPt = { 0, 0 };
    POINT currentPt = { 0, 0 };
    RECT selectedRect = { 0, 0, 0, 0 };
    bool isFrozen = false;
    HBITMAP hOriginalBmp = NULL;
    HBITMAP hDimmedBmp = NULL;
    int screenX = 0;
    int screenY = 0;
    int screenW = 0;
    int screenH = 0;
    HWND hOverlayWnd = NULL;
} g_sel;

// Прототипы
void TriggerFullScreenCapture();
void TriggerLiveAreaCapture();
void TriggerFrozenAreaCapture();
void ShowNotification(const std::wstring& title, const std::wstring& message);
void AutoInstallIfNeeded();
bool IsAutostartEnabled();
void SetAutostart(bool enable);
void UninstallApp();
void OpenScreenshotsFolder();
void OpenSettingsDialog();

// Получение системной папки скриншотов
std::wstring GetDefaultScreenshotsDir() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYPICTURES, NULL, 0, path))) {
        std::wstring s(path);
        s += L"\\Скриншоты";
        CreateDirectoryW(s.c_str(), NULL);
        return s;
    }
    return L"C:\\Скриншоты";
}

// Получение системной папки видеозаписей
std::wstring GetDefaultRecordingsDir() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYVIDEO, NULL, 0, path))) {
        std::wstring s(path);
        s += L"\\Записи WinScreen";
        CreateDirectoryW(s.c_str(), NULL);
        return s;
    }
    return L"C:\\Записи WinScreen";
}

// Получение директории AppData
std::wstring GetAppDataDir() {
    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring s(path);
        s += L"\\WinScreen";
        CreateDirectoryW(s.c_str(), NULL);
        return s;
    }
    return L".";
}

// Поиск исполняемого файла FFmpeg
std::wstring GetFFmpegPath() {
    // 1. Рядом с запущенным исполняемым файлом
    WCHAR exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        for (int i = (int)wcslen(exePath) - 1; i >= 0; --i) {
            if (exePath[i] == L'\\' || exePath[i] == L'/') {
                exePath[i] = 0;
                break;
            }
        }
        std::wstring p1 = std::wstring(exePath) + L"\\ffmpeg.exe";
        if (GetFileAttributesW(p1.c_str()) != INVALID_FILE_ATTRIBUTES) return p1;
    }

    // 2. В папке %APPDATA%\WinScreen\ffmpeg.exe
    std::wstring p2 = GetAppDataDir() + L"\\ffmpeg.exe";
    if (GetFileAttributesW(p2.c_str()) != INVALID_FILE_ATTRIBUTES) return p2;

    // 3. В системном PATH
    WCHAR foundPath[MAX_PATH];
    if (SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, foundPath, NULL) > 0) {
        return foundPath;
    }

    return L"";
}

bool IsFFmpegInstalled() {
    return !GetFFmpegPath().empty();
}

struct DownloadParams {
    HWND hDlg;
    HWND hBtn;
};

DWORD WINAPI DownloadFFmpegThread(LPVOID lpParam) {
    DownloadParams* dp = (DownloadParams*)lpParam;
    std::wstring destDir = GetAppDataDir();
    CreateDirectoryW(destDir.c_str(), NULL);
    std::wstring dest = destDir + L"\\ffmpeg.exe";
    std::wstring tempDest = dest + L".tmp";

    const WCHAR* url = L"https://github.com/imageio/imageio-binaries/raw/master/ffmpeg/ffmpeg-win64-v4.2.2.exe";

    HRESULT hr = URLDownloadToFileW(NULL, url, tempDest.c_str(), 0, NULL);
    if (SUCCEEDED(hr)) {
        MoveFileExW(tempDest.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING);
        if (dp && dp->hDlg) {
            MessageBoxW(dp->hDlg, L"FFmpeg успешно установлен!\nВидеозапись теперь готова к работе.", L"WinScreen", MB_OK | MB_ICONINFORMATION);
        } else {
            ShowNotification(L"FFmpeg установлен", L"FFmpeg успешно установлен. Видеозапись готова к работе.");
        }
        if (dp && dp->hBtn && IsWindow(dp->hBtn)) {
            ShowWindow(dp->hBtn, SW_HIDE);
        }
    } else {
        DeleteFileW(tempDest.c_str());
        if (dp && dp->hDlg) {
            MessageBoxW(dp->hDlg, L"Не удалось скачать FFmpeg. Пожалуйста, проверьте подключение к Интернету.", L"Ошибка загрузки FFmpeg", MB_OK | MB_ICONERROR);
        } else {
            ShowNotification(L"Ошибка загрузки", L"Не удалось скачать FFmpeg. Проверьте интернет.");
        }
        if (dp && dp->hBtn && IsWindow(dp->hBtn)) {
            EnableWindow(dp->hBtn, TRUE);
            SetWindowTextW(dp->hBtn, L"📥 Установить FFmpeg");
        }
    }
    if (dp) delete dp;
    return 0;
}

void ParseHotkeyConfig(const std::wstring& val, HotkeyConfig& hk) {
    std::wstringstream ss(val);
    std::wstring item;
    std::vector<std::wstring> tokens;
    while (std::getline(ss, item, L',')) {
        tokens.push_back(item);
    }
    if (tokens.size() >= 4) {
        hk.vkCode = (DWORD)_wtoi(tokens[0].c_str());
        hk.ctrl = (tokens[1] == L"1");
        hk.shift = (tokens[2] == L"1");
        hk.alt = (tokens[3] == L"1");
        hk.win = (tokens.size() >= 5 && tokens[4] == L"1");
        hk.displayText = FormatHotkey(hk.vkCode, hk.ctrl, hk.shift, hk.alt, hk.win);
    }
}

// Загрузка и сохранение настроек
void LoadSettings() {
    g_saveDir = GetDefaultScreenshotsDir();
    g_videoSaveDir = GetDefaultRecordingsDir();
    std::wstring cfgFile = GetAppDataDir() + L"\\config.txt";
    std::wifstream fin(cfgFile);
    if (fin.is_open()) {
        std::wstring line;
        while (std::getline(fin, line)) {
            size_t eq = line.find(L'=');
            if (eq != std::wstring::npos) {
                std::wstring key = line.substr(0, eq);
                std::wstring val = line.substr(eq + 1);
                if (key == L"dir" && !val.empty()) g_saveDir = val;
                else if (key == L"video_dir" && !val.empty()) g_videoSaveDir = val;
                else if (key == L"format") g_fileFormat = val;
                else if (key == L"quality") g_jpgQuality = max(1, min(100, _wtoi(val.c_str())));
                else if (key == L"clipboard") g_copyClipboard = (val == L"1");
                else if (key == L"disk") g_saveDisk = (val == L"1");
                else if (key == L"notify") g_showNotifications = (val == L"1");
                else if (key == L"hk_full") ParseHotkeyConfig(val, g_hkFullscreen);
                else if (key == L"hk_live") ParseHotkeyConfig(val, g_hkLive);
                else if (key == L"hk_frozen") ParseHotkeyConfig(val, g_hkFrozen);
                else if (key == L"hk_record") ParseHotkeyConfig(val, g_hkRecord);
                else if (key == L"record_system_audio") g_recordSysAudio = (val == L"1");
                else if (key == L"record_microphone") g_recordMic = (val == L"1");
                else if (key == L"selected_microphone") g_selectedMic = val;
            }
        }
    }
    CreateDirectoryW(g_saveDir.c_str(), NULL);
    CreateDirectoryW(g_videoSaveDir.c_str(), NULL);
}

void SaveSettings() {
    std::wstring cfgFile = GetAppDataDir() + L"\\config.txt";
    std::wofstream fout(cfgFile);
    if (fout.is_open()) {
        fout << L"dir=" << g_saveDir << L"\n";
        fout << L"video_dir=" << g_videoSaveDir << L"\n";
        fout << L"format=" << g_fileFormat << L"\n";
        fout << L"quality=" << g_jpgQuality << L"\n";
        fout << L"clipboard=" << (g_copyClipboard ? 1 : 0) << L"\n";
        fout << L"disk=" << (g_saveDisk ? 1 : 0) << L"\n";
        fout << L"notify=" << (g_showNotifications ? 1 : 0) << L"\n";
        fout << L"record_system_audio=" << (g_recordSysAudio ? 1 : 0) << L"\n";
        fout << L"record_microphone=" << (g_recordMic ? 1 : 0) << L"\n";
        fout << L"selected_microphone=" << g_selectedMic << L"\n";
        fout << L"hk_full=" << g_hkFullscreen.vkCode << L"," << (g_hkFullscreen.ctrl?1:0) << L"," << (g_hkFullscreen.shift?1:0) << L"," << (g_hkFullscreen.alt?1:0) << L"," << (g_hkFullscreen.win?1:0) << L"\n";
        fout << L"hk_live=" << g_hkLive.vkCode << L"," << (g_hkLive.ctrl?1:0) << L"," << (g_hkLive.shift?1:0) << L"," << (g_hkLive.alt?1:0) << L"," << (g_hkLive.win?1:0) << L"\n";
        fout << L"hk_frozen=" << g_hkFrozen.vkCode << L"," << (g_hkFrozen.ctrl?1:0) << L"," << (g_hkFrozen.shift?1:0) << L"," << (g_hkFrozen.alt?1:0) << L"," << (g_hkFrozen.win?1:0) << L"\n";
        fout << L"hk_record=" << g_hkRecord.vkCode << L"," << (g_hkRecord.ctrl?1:0) << L"," << (g_hkRecord.shift?1:0) << L"," << (g_hkRecord.alt?1:0) << L"," << (g_hkRecord.win?1:0) << L"\n";
    }
}

// Оповещения через системный трей (нативные тосты Windows 10/11)
void ShowNotification(const std::wstring& title, const std::wstring& message) {
    if (!g_showNotifications) return;
    NOTIFYICONDATAW nid = g_nid;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcsncpy(nid.szInfoTitle, title.c_str(), sizeof(nid.szInfoTitle) / sizeof(WCHAR) - 1);
    wcsncpy(nid.szInfo, message.c_str(), sizeof(nid.szInfo) / sizeof(WCHAR) - 1);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// Копирование HBITMAP в буфер обмена в формате CF_DIB
bool CopyBitmapToClipboard(HBITMAP hBitmap) {
    if (!hBitmap) return false;
    HDC hDC = GetDC(NULL);
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    DWORD dwSize = ((bmp.bmWidth * 32 + 31) / 32) * 4 * bmp.bmHeight;

    HANDLE hDIB = GlobalAlloc(GHND, sizeof(BITMAPINFOHEADER) + dwSize);
    if (!hDIB) {
        ReleaseDC(NULL, hDC);
        return false;
    }

    LPBYTE lpDIB = (LPBYTE)GlobalLock(hDIB);
    CopyMemory(lpDIB, &bi, sizeof(BITMAPINFOHEADER));
    GetDIBits(hDC, hBitmap, 0, bmp.bmHeight, lpDIB + sizeof(BITMAPINFOHEADER), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    GlobalUnlock(hDIB);
    ReleaseDC(NULL, hDC);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_DIB, hDIB);
        CloseClipboard();
        return true;
    }
    GlobalFree(hDIB);
    return false;
}

// Поиск CLSID кодировщика GDI+
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// Сохранение Bitmap на диск
std::wstring SaveBitmapToDisk(HBITMAP hBitmap) {
    if (!hBitmap) return L"";
    CreateDirectoryW(g_saveDir.c_str(), NULL);

    time_t rawtime;
    struct tm* timeinfo;
    WCHAR timeBuf[64];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    wcsftime(timeBuf, 64, L"Скриншот_%Y-%m-%d_%H-%M-%S", timeinfo);

    std::wstring ext = (g_fileFormat == L"jpg" || g_fileFormat == L"jpeg") ? L".jpg" : L".png";
    std::wstring filePath = g_saveDir + L"\\" + timeBuf + ext;

    int counter = 1;
    while (GetFileAttributesW(filePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        filePath = g_saveDir + L"\\" + timeBuf + L"_" + std::to_wstring(counter++) + ext;
    }

    Bitmap bmp(hBitmap, NULL);
    CLSID encoderClsid;

    if (ext == L".jpg") {
        if (GetEncoderClsid(L"image/jpeg", &encoderClsid) >= 0) {
            EncoderParameters encoderParameters;
            encoderParameters.Count = 1;
            encoderParameters.Parameter[0].Guid = EncoderQuality;
            encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
            encoderParameters.Parameter[0].NumberOfValues = 1;
            ULONG quality = (ULONG)g_jpgQuality;
            encoderParameters.Parameter[0].Value = &quality;
            bmp.Save(filePath.c_str(), &encoderClsid, &encoderParameters);
        }
    } else {
        if (GetEncoderClsid(L"image/png", &encoderClsid) >= 0) {
            bmp.Save(filePath.c_str(), &encoderClsid, NULL);
        }
    }
    return filePath;
}

// Единая обработка созданного снимка
void ProcessCapturedBitmap(HBITMAP hBitmap) {
    if (!hBitmap) {
        ShowNotification(L"Ошибка создания снимка", L"Не удалось захватить изображение");
        return;
    }

    if (g_copyClipboard) {
        CopyBitmapToClipboard(hBitmap);
    }

    std::wstring savedPath = L"";
    if (g_saveDisk) {
        savedPath = SaveBitmapToDisk(hBitmap);
    }

    DeleteObject(hBitmap);
    ShowNotification(L"Скриншот готов", L"Скопирован в буфер и сохранён в папку");
}

// Захват виртуального экрана (всех мониторов)
HBITMAP CaptureVirtualScreen() {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, vw, vh);
    HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, vw, vh, hScreenDC, vx, vy, SRCCOPY | CAPTUREBLT);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}

// Создание затемненной копии битмапа для режима заморозки
HBITMAP CreateDimmedBitmap(HBITMAP hSrc, int w, int h) {
    HDC hScreenDC = GetDC(NULL);
    HDC hSrcDC = CreateCompatibleDC(hScreenDC);
    HDC hDstDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hDst = CreateCompatibleBitmap(hScreenDC, w, h);

    HGDIOBJ hOldSrc = SelectObject(hSrcDC, hSrc);
    HGDIOBJ hOldDst = SelectObject(hDstDC, hDst);

    BitBlt(hDstDC, 0, 0, w, h, hSrcDC, 0, 0, SRCCOPY);

    // Накладываем полупрозрачное затемнение (BLENDFUNCTION)
    HDC hBlackDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBlack = CreateCompatibleBitmap(hScreenDC, w, h);
    HGDIOBJ hOldBlack = SelectObject(hBlackDC, hBlack);
    RECT rc = { 0, 0, w, h };
    FillRect(hBlackDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 110, 0 }; // 43% затемнение
    AlphaBlend(hDstDC, 0, 0, w, h, hBlackDC, 0, 0, w, h, bf);

    SelectObject(hBlackDC, hOldBlack);
    DeleteObject(hBlack);
    DeleteDC(hBlackDC);

    SelectObject(hDstDC, hOldDst);
    SelectObject(hSrcDC, hOldSrc);
    DeleteDC(hDstDC);
    DeleteDC(hSrcDC);
    ReleaseDC(NULL, hScreenDC);

    return hDst;
}

// Режим 1: Полный экран
void TriggerFullScreenCapture() {
    HBITMAP hBmp = CaptureVirtualScreen();
    ProcessCapturedBitmap(hBmp);
}

// Оконная процедура оверлея
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        HDC hMemDC = CreateCompatibleDC(hdc);

        if (g_sel.isFrozen && g_sel.hDimmedBmp) {
            // Рисуем затемненный фон
            SelectObject(hMemDC, g_sel.hDimmedBmp);
            BitBlt(hdc, 0, 0, g_sel.screenW, g_sel.screenH, hMemDC, 0, 0, SRCCOPY);

            // Если выделена область, вырезаем яркий исходный кадр внутри рамки
            if (g_sel.isSelecting) {
                int x1 = min(g_sel.startPt.x, g_sel.currentPt.x);
                int y1 = min(g_sel.startPt.y, g_sel.currentPt.y);
                int x2 = max(g_sel.startPt.x, g_sel.currentPt.x);
                int y2 = max(g_sel.startPt.y, g_sel.currentPt.y);
                int rw = x2 - x1;
                int rh = y2 - y1;

                if (rw > 0 && rh > 0 && g_sel.hOriginalBmp) {
                    SelectObject(hMemDC, g_sel.hOriginalBmp);
                    BitBlt(hdc, x1, y1, rw, rh, hMemDC, x1, y1, SRCCOPY);

                    // Чистая неоновая рамка (строгий минимализм, без лишних цифр и луп)
                    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 229, 255));
                    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
                    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, x1, y1, x2, y2);
                    SelectObject(hdc, hOldBrush);
                    SelectObject(hdc, hOldPen);
                    DeleteObject(hPen);
                }
            }
        } else if (!g_sel.isFrozen) {
            // В живом режиме: рисуем аккуратную рамку
            if (g_sel.isSelecting) {
                int x1 = min(g_sel.startPt.x, g_sel.currentPt.x);
                int y1 = min(g_sel.startPt.y, g_sel.currentPt.y);
                int x2 = max(g_sel.startPt.x, g_sel.currentPt.x);
                int y2 = max(g_sel.startPt.y, g_sel.currentPt.y);
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 210, 255));
                HGDIOBJ hOldPen = SelectObject(hdc, hPen);
                HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, x1, y1, x2, y2);
                SelectObject(hdc, hOldBrush);
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            }
        }

        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        g_sel.isSelecting = true;
        g_sel.startPt.x = GET_X_LPARAM(lParam);
        g_sel.startPt.y = GET_Y_LPARAM(lParam);
        g_sel.currentPt = g_sel.startPt;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_sel.isSelecting) {
            g_sel.currentPt.x = GET_X_LPARAM(lParam);
            g_sel.currentPt.y = GET_Y_LPARAM(lParam);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!g_sel.isSelecting) return 0;
        ReleaseCapture();
        g_sel.isSelecting = false;

        int x1 = min(g_sel.startPt.x, g_sel.currentPt.x);
        int y1 = min(g_sel.startPt.y, g_sel.currentPt.y);
        int x2 = max(g_sel.startPt.x, g_sel.currentPt.x);
        int y2 = max(g_sel.startPt.y, g_sel.currentPt.y);
        int rw = x2 - x1;
        int rh = y2 - y1;

        DestroyWindow(hWnd);

        if (rw < 5 || rh < 5) {
            ShowNotification(L"Снимок отменён", L"Выделение области прервано");
            return 0;
        }

        HBITMAP hCrop = NULL;
        if (g_sel.isFrozen && g_sel.hOriginalBmp) {
            // Вырезаем фрагмент из замороженного в памяти стоп-кадра
            HDC hScreenDC = GetDC(NULL);
            HDC hSrcDC = CreateCompatibleDC(hScreenDC);
            HDC hDstDC = CreateCompatibleDC(hScreenDC);
            hCrop = CreateCompatibleBitmap(hScreenDC, rw, rh);
            HGDIOBJ hOldSrc = SelectObject(hSrcDC, g_sel.hOriginalBmp);
            HGDIOBJ hOldDst = SelectObject(hDstDC, hCrop);

            BitBlt(hDstDC, 0, 0, rw, rh, hSrcDC, x1, y1, SRCCOPY);

            SelectObject(hDstDC, hOldDst);
            SelectObject(hSrcDC, hOldSrc);
            DeleteDC(hDstDC);
            DeleteDC(hSrcDC);
            ReleaseDC(NULL, hScreenDC);
        } else {
            // Захватываем живую область экрана
            Sleep(25); // даем оверлею скрыться
            int absX = g_sel.screenX + x1;
            int absY = g_sel.screenY + y1;
            HDC hScreenDC = GetDC(NULL);
            HDC hMemDC = CreateCompatibleDC(hScreenDC);
            hCrop = CreateCompatibleBitmap(hScreenDC, rw, rh);
            HGDIOBJ hOld = SelectObject(hMemDC, hCrop);

            BitBlt(hMemDC, 0, 0, rw, rh, hScreenDC, absX, absY, SRCCOPY | CAPTUREBLT);

            SelectObject(hMemDC, hOld);
            DeleteDC(hMemDC);
            ReleaseDC(NULL, hScreenDC);
        }

        ProcessCapturedBitmap(hCrop);
        return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_KEYDOWN: {
        if (msg == WM_RBUTTONDOWN || wParam == VK_ESCAPE) {
            ReleaseCapture();
            g_sel.isSelecting = false;
            DestroyWindow(hWnd);
            ShowNotification(L"Снимок отменён", L"Выделение области прервано");
        }
        return 0;
    }
    case WM_DESTROY: {
        if (g_sel.hOriginalBmp) {
            DeleteObject(g_sel.hOriginalBmp);
            g_sel.hOriginalBmp = NULL;
        }
        if (g_sel.hDimmedBmp) {
            DeleteObject(g_sel.hDimmedBmp);
            g_sel.hDimmedBmp = NULL;
        }
        g_sel.hOverlayWnd = NULL;
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Запуск оверлея
void StartOverlay(bool isFrozen) {
    if (g_sel.hOverlayWnd && IsWindow(g_sel.hOverlayWnd)) return;

    g_sel.isFrozen = isFrozen;
    g_sel.isSelecting = false;
    g_sel.screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_sel.screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_sel.screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_sel.screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (isFrozen) {
        // Мгновенный стоп-кадр в миллисекунду вызова
        g_sel.hOriginalBmp = CaptureVirtualScreen();
        g_sel.hDimmedBmp = CreateDimmedBitmap(g_sel.hOriginalBmp, g_sel.screenW, g_sel.screenH);
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"WinScreen_Overlay";
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    if (!isFrozen) {
        exStyle |= WS_EX_LAYERED;
    }

    g_sel.hOverlayWnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"WinScreen Overlay",
        WS_POPUP | WS_VISIBLE,
        g_sel.screenX, g_sel.screenY, g_sel.screenW, g_sel.screenH,
        NULL, NULL, g_hInstance, NULL
    );

    if (!isFrozen) {
        // Полупрозрачный черный фон для живого выделения
        SetLayeredWindowAttributes(g_sel.hOverlayWnd, 0, 75, LWA_ALPHA);
    }

    SetForegroundWindow(g_sel.hOverlayWnd);
    SetFocus(g_sel.hOverlayWnd);

    MSG msg;
    while (IsWindow(g_sel.hOverlayWnd) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

struct AudioDevices {
    std::vector<std::wstring> microphones;
    std::wstring systemAudio;
};

inline std::wstring SafeAudioName(const std::wstring& s) {
    std::wstring res;
    for (WCHAR c : s) {
        if (c != L'"') res += c;
    }
    return res;
}

AudioDevices EnumerateAudioDevices() {
    AudioDevices devs;
    HRESULT hr = CoInitialize(NULL);

    ICreateDevEnum* pDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    if (SUCCEEDED(hr) && pDevEnum) {
        IEnumMoniker* pEnum = NULL;
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnum, 0);
        if (hr == S_OK && pEnum) {
            IMoniker* pMoniker = NULL;
            while (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
                IPropertyBag* pPropBag = NULL;
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
                if (SUCCEEDED(hr) && pPropBag) {
                    VARIANT var;
                    VariantInit(&var);
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != NULL) {
                        std::wstring name(var.bstrVal);
                        std::wstring lower = name;
                        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
                        if (lower.find(L"stereo mix") != std::wstring::npos ||
                            lower.find(L"стерео микшер") != std::wstring::npos ||
                            lower.find(L"стерео микс") != std::wstring::npos ||
                            lower.find(L"what u hear") != std::wstring::npos ||
                            lower.find(L"wave out") != std::wstring::npos ||
                            lower.find(L"cable output") != std::wstring::npos ||
                            lower.find(L"virtual-audio-capturer") != std::wstring::npos) {
                            if (devs.systemAudio.empty()) {
                                devs.systemAudio = name;
                            }
                        } else {
                            if (std::find(devs.microphones.begin(), devs.microphones.end(), name) == devs.microphones.end()) {
                                devs.microphones.push_back(name);
                            }
                        }
                    }
                    VariantClear(&var);
                    pPropBag->Release();
                }
                pMoniker->Release();
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }
    CoUninitialize();
    return devs;
}

// Состояние видеозаписи и захвата звука (WASAPI)
std::atomic<bool> g_wasapiRecording(false);
std::thread g_wasapiThread;
std::wstring g_currentTempVideoPath;
std::wstring g_currentTempAudioPath;
std::wstring g_currentFinalVideoPath;
bool g_isMuxingRequired = false;

struct WasapiCaptureParams {
    std::wstring wavPath;
    bool recordSys = false;
    bool recordMic = false;
    std::wstring selectedMic = L"";
};

// Проверка, является ли формат IEEE float
inline bool IsWaveFormatFloat(const WAVEFORMATEX* wfx) {
    if (!wfx) return false;
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* pExt = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (pExt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) return true;
    }
    return false;
}

// Единый поток нативного захвата звука (системного loopback и/или микрофона) через WASAPI
void WasapiAudioCaptureThread(WasapiCaptureParams params) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    IMMDeviceEnumerator* pEnumerator = NULL;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        CoUninitialize();
        return;
    }

    // 1. Инициализация захвата системного звука (WASAPI Loopback)
    IMMDevice* pSysDevice = NULL;
    IAudioClient* pSysClient = NULL;
    IAudioCaptureClient* pSysCapture = NULL;
    IAudioClient* pSilentClient = NULL;
    IAudioRenderClient* pSilentRender = NULL;
    WAVEFORMATEX* pSysWfx = NULL;
    UINT32 silentBufferFrames = 0;
    bool isSysFloat = false;
    WORD sysChannels = 2;

    if (params.recordSys) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pSysDevice);
        if (SUCCEEDED(hr) && pSysDevice) {
            hr = pSysDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pSysClient);
            if (SUCCEEDED(hr) && pSysClient) {
                pSysClient->GetMixFormat(&pSysWfx);
                if (pSysWfx) {
                    isSysFloat = IsWaveFormatFloat(pSysWfx);
                    sysChannels = pSysWfx->nChannels;

                    // Запускаем фоновый render-клиент тишины на том же устройстве.
                    // Это заставляет аудиодвижок Windows непрерывно рендерить буферы,
                    // устраняя засыпание endpoint'а и гарантируя мгновенную доставку пакетов.
                    if (SUCCEEDED(pSysDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pSilentClient)) && pSilentClient) {
                        if (SUCCEEDED(pSilentClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pSysWfx, NULL))) {
                            if (SUCCEEDED(pSilentClient->GetService(__uuidof(IAudioRenderClient), (void**)&pSilentRender)) && pSilentRender) {
                                pSilentClient->GetBufferSize(&silentBufferFrames);
                                BYTE* pSil = NULL;
                                if (SUCCEEDED(pSilentRender->GetBuffer(silentBufferFrames, &pSil)) && pSil) {
                                    memset(pSil, 0, silentBufferFrames * pSysWfx->nBlockAlign);
                                    pSilentRender->ReleaseBuffer(silentBufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
                                }
                                pSilentClient->Start();
                            }
                        }
                    }

                    hr = pSysClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                                10000000, 0, pSysWfx, NULL);
                    if (SUCCEEDED(hr)) {
                        pSysClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pSysCapture);
                        if (pSysCapture) {
                            pSysClient->Start();
                        }
                    }
                }
            }
        }
    }

    // 2. Инициализация захвата микрофона (WASAPI Capture)
    IMMDevice* pMicDevice = NULL;
    IAudioClient* pMicClient = NULL;
    IAudioCaptureClient* pMicCapture = NULL;
    WAVEFORMATEX* pMicWfx = NULL;
    bool isMicFloat = false;
    WORD micChannels = 1;

    if (params.recordMic) {
        if (!params.selectedMic.empty()) {
            IMMDeviceCollection* pMicCol = NULL;
            if (SUCCEEDED(pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pMicCol)) && pMicCol) {
                UINT count = 0;
                pMicCol->GetCount(&count);
                PROPERTYKEY pkFriendly = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice* pDev = NULL;
                    if (SUCCEEDED(pMicCol->Item(i, &pDev)) && pDev) {
                        IPropertyStore* pProps = NULL;
                        if (SUCCEEDED(pDev->OpenPropertyStore(STGM_READ, &pProps)) && pProps) {
                            PROPVARIANT pv;
                            PropVariantInit(&pv);
                            if (SUCCEEDED(pProps->GetValue(pkFriendly, &pv)) && pv.pwszVal) {
                                if (params.selectedMic == pv.pwszVal) {
                                    pMicDevice = pDev;
                                    pDev->AddRef();
                                }
                            }
                            PropVariantClear(&pv);
                            pProps->Release();
                        }
                        pDev->Release();
                        if (pMicDevice) break;
                    }
                }
                pMicCol->Release();
            }
        }
        if (!pMicDevice) {
            pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pMicDevice);
        }

        if (pMicDevice) {
            hr = pMicDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pMicClient);
            if (SUCCEEDED(hr) && pMicClient) {
                pMicClient->GetMixFormat(&pMicWfx);
                if (pMicWfx) {
                    isMicFloat = IsWaveFormatFloat(pMicWfx);
                    micChannels = pMicWfx->nChannels;

                    hr = pMicClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, pMicWfx, NULL);
                    if (SUCCEEDED(hr)) {
                        pMicClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pMicCapture);
                        if (pMicCapture) {
                            pMicClient->Start();
                        }
                    }
                }
            }
        }
    }

    // Если ни один из источников не смог запуститься
    if (!pSysCapture && !pMicCapture) {
        if (pSilentClient) { pSilentClient->Stop(); pSilentClient->Release(); }
        if (pSilentRender) pSilentRender->Release();
        if (pSysClient) { pSysClient->Stop(); pSysClient->Release(); }
        if (pSysCapture) pSysCapture->Release();
        if (pSysDevice) pSysDevice->Release();
        if (pSysWfx) CoTaskMemFree(pSysWfx);
        if (pMicClient) { pMicClient->Stop(); pMicClient->Release(); }
        if (pMicCapture) pMicCapture->Release();
        if (pMicDevice) pMicDevice->Release();
        if (pMicWfx) CoTaskMemFree(pMicWfx);
        pEnumerator->Release();
        CoUninitialize();
        return;
    }

    // Формат выходного WAV-файла (стерео 16-бит PCM)
    DWORD outSampleRate = 48000;
    if (pSysWfx && pSysWfx->nSamplesPerSec > 0) outSampleRate = pSysWfx->nSamplesPerSec;
    else if (pMicWfx && pMicWfx->nSamplesPerSec > 0) outSampleRate = pMicWfx->nSamplesPerSec;

    WORD outChannels = 2;
    WORD outBitsPerSample = 16;
    WORD outBlockAlign = outChannels * (outBitsPerSample / 8);
    DWORD outByteRate = outSampleRate * outBlockAlign;

    std::ofstream wavFile(params.wavPath, std::ios::binary);
    if (!wavFile.is_open()) {
        if (pSilentClient) { pSilentClient->Stop(); pSilentClient->Release(); }
        if (pSilentRender) pSilentRender->Release();
        if (pSysClient) { pSysClient->Stop(); pSysClient->Release(); }
        if (pSysCapture) pSysCapture->Release();
        if (pSysDevice) pSysDevice->Release();
        if (pSysWfx) CoTaskMemFree(pSysWfx);
        if (pMicClient) { pMicClient->Stop(); pMicClient->Release(); }
        if (pMicCapture) pMicCapture->Release();
        if (pMicDevice) pMicDevice->Release();
        if (pMicWfx) CoTaskMemFree(pMicWfx);
        pEnumerator->Release();
        CoUninitialize();
        return;
    }

    // Записываем заголовок WAV (44 байта)
    char header[44] = { 0 };
    memcpy(header, "RIFF", 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    DWORD subchunk1Size = 16; // PCM
    memcpy(header + 16, &subchunk1Size, 4);
    WORD audioFormat = 1; // PCM
    memcpy(header + 20, &audioFormat, 2);
    memcpy(header + 22, &outChannels, 2);
    memcpy(header + 24, &outSampleRate, 4);
    memcpy(header + 28, &outByteRate, 4);
    memcpy(header + 32, &outBlockAlign, 2);
    memcpy(header + 34, &outBitsPerSample, 2);
    memcpy(header + 36, "data", 4);
    wavFile.write(header, 44);

    std::deque<float> sysQueueL, sysQueueR;
    std::deque<float> micQueueL, micQueueR;
    uint32_t totalBytesWritten = 0;

    while (g_wasapiRecording.load()) {
        // Поддерживаем render-клиент тишины активным
        if (pSilentClient && pSilentRender && silentBufferFrames > 0 && pSysWfx) {
            UINT32 padding = 0;
            if (SUCCEEDED(pSilentClient->GetCurrentPadding(&padding))) {
                UINT32 needed = (silentBufferFrames > padding) ? (silentBufferFrames - padding) : 0;
                if (needed > 0) {
                    BYTE* pSil = NULL;
                    if (SUCCEEDED(pSilentRender->GetBuffer(needed, &pSil)) && pSil) {
                        memset(pSil, 0, needed * pSysWfx->nBlockAlign);
                        pSilentRender->ReleaseBuffer(needed, AUDCLNT_BUFFERFLAGS_SILENT);
                    }
                }
            }
        }

        // Читаем доступные пакеты системного звука
        if (pSysCapture) {
            UINT32 packetLen = 0;
            while (SUCCEEDED(pSysCapture->GetNextPacketSize(&packetLen)) && packetLen > 0) {
                BYTE* pData = NULL;
                UINT32 numFrames = 0;
                DWORD flags = 0;
                if (SUCCEEDED(pSysCapture->GetBuffer(&pData, &numFrames, &flags, NULL, NULL))) {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            sysQueueL.push_back(0.0f);
                            sysQueueR.push_back(0.0f);
                        }
                    } else if (isSysFloat) {
                        const float* f = reinterpret_cast<const float*>(pData);
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            if (sysChannels >= 2) {
                                sysQueueL.push_back(f[i * sysChannels]);
                                sysQueueR.push_back(f[i * sysChannels + 1]);
                            } else {
                                sysQueueL.push_back(f[i]);
                                sysQueueR.push_back(f[i]);
                            }
                        }
                    } else {
                        const int16_t* s = reinterpret_cast<const int16_t*>(pData);
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            if (sysChannels >= 2) {
                                sysQueueL.push_back(s[i * sysChannels] / 32768.0f);
                                sysQueueR.push_back(s[i * sysChannels + 1] / 32768.0f);
                            } else {
                                sysQueueL.push_back(s[i] / 32768.0f);
                                sysQueueR.push_back(s[i] / 32768.0f);
                            }
                        }
                    }
                    pSysCapture->ReleaseBuffer(numFrames);
                } else {
                    break;
                }
            }
        }

        // Читаем доступные пакеты микрофона
        if (pMicCapture) {
            UINT32 packetLen = 0;
            while (SUCCEEDED(pMicCapture->GetNextPacketSize(&packetLen)) && packetLen > 0) {
                BYTE* pData = NULL;
                UINT32 numFrames = 0;
                DWORD flags = 0;
                if (SUCCEEDED(pMicCapture->GetBuffer(&pData, &numFrames, &flags, NULL, NULL))) {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            micQueueL.push_back(0.0f);
                            micQueueR.push_back(0.0f);
                        }
                    } else if (isMicFloat) {
                        const float* f = reinterpret_cast<const float*>(pData);
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            if (micChannels >= 2) {
                                micQueueL.push_back(f[i * micChannels]);
                                micQueueR.push_back(f[i * micChannels + 1]);
                            } else {
                                micQueueL.push_back(f[i]);
                                micQueueR.push_back(f[i]);
                            }
                        }
                    } else {
                        const int16_t* s = reinterpret_cast<const int16_t*>(pData);
                        for (UINT32 i = 0; i < numFrames; ++i) {
                            if (micChannels >= 2) {
                                micQueueL.push_back(s[i * micChannels] / 32768.0f);
                                micQueueR.push_back(s[i * micChannels + 1] / 32768.0f);
                            } else {
                                micQueueL.push_back(s[i] / 32768.0f);
                                micQueueR.push_back(s[i] / 32768.0f);
                            }
                        }
                    }
                    pMicCapture->ReleaseBuffer(numFrames);
                } else {
                    break;
                }
            }
        }

        // Сведение и запись в файл
        if (pSysCapture && pMicCapture) {
            size_t canWrite = std::min(sysQueueL.size(), micQueueL.size());
            if (canWrite > 0) {
                std::vector<int16_t> pcm(canWrite * 2);
                for (size_t i = 0; i < canWrite; ++i) {
                    float sL = sysQueueL.front(); sysQueueL.pop_front();
                    float sR = sysQueueR.front(); sysQueueR.pop_front();
                    float mL = micQueueL.front(); micQueueL.pop_front();
                    float mR = micQueueR.front(); micQueueR.pop_front();

                    int32_t mixL = static_cast<int32_t>((sL + mL) * 32767.0f);
                    int32_t mixR = static_cast<int32_t>((sR + mR) * 32767.0f);
                    if (mixL > 32767) mixL = 32767; else if (mixL < -32768) mixL = -32768;
                    if (mixR > 32767) mixR = 32767; else if (mixR < -32768) mixR = -32768;

                    pcm[i * 2] = static_cast<int16_t>(mixL);
                    pcm[i * 2 + 1] = static_cast<int16_t>(mixR);
                }
                wavFile.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(int16_t));
                totalBytesWritten += static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
            }
        } else if (pSysCapture) {
            size_t canWrite = sysQueueL.size();
            if (canWrite > 0) {
                std::vector<int16_t> pcm(canWrite * 2);
                for (size_t i = 0; i < canWrite; ++i) {
                    float sL = sysQueueL.front(); sysQueueL.pop_front();
                    float sR = sysQueueR.front(); sysQueueR.pop_front();
                    if (sL > 1.0f) sL = 1.0f; else if (sL < -1.0f) sL = -1.0f;
                    if (sR > 1.0f) sR = 1.0f; else if (sR < -1.0f) sR = -1.0f;

                    pcm[i * 2] = static_cast<int16_t>(sL * 32767.0f);
                    pcm[i * 2 + 1] = static_cast<int16_t>(sR * 32767.0f);
                }
                wavFile.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(int16_t));
                totalBytesWritten += static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
            }
        } else if (pMicCapture) {
            size_t canWrite = micQueueL.size();
            if (canWrite > 0) {
                std::vector<int16_t> pcm(canWrite * 2);
                for (size_t i = 0; i < canWrite; ++i) {
                    float mL = micQueueL.front(); micQueueL.pop_front();
                    float mR = micQueueR.front(); micQueueR.pop_front();
                    if (mL > 1.0f) mL = 1.0f; else if (mL < -1.0f) mL = -1.0f;
                    if (mR > 1.0f) mR = 1.0f; else if (mR < -1.0f) mR = -1.0f;

                    pcm[i * 2] = static_cast<int16_t>(mL * 32767.0f);
                    pcm[i * 2 + 1] = static_cast<int16_t>(mR * 32767.0f);
                }
                wavFile.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(int16_t));
                totalBytesWritten += static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
            }
        }

        Sleep(5);
    }

    // Записываем оставшиеся в очередях сэмплы
    while (!sysQueueL.empty() || !micQueueL.empty()) {
        float sL = sysQueueL.empty() ? 0.0f : sysQueueL.front();
        float sR = sysQueueR.empty() ? 0.0f : sysQueueR.front();
        if (!sysQueueL.empty()) { sysQueueL.pop_front(); sysQueueR.pop_front(); }

        float mL = micQueueL.empty() ? 0.0f : micQueueL.front();
        float mR = micQueueR.empty() ? 0.0f : micQueueR.front();
        if (!micQueueL.empty()) { micQueueL.pop_front(); micQueueR.pop_front(); }

        int32_t mixL = static_cast<int32_t>((sL + mL) * 32767.0f);
        int32_t mixR = static_cast<int32_t>((sR + mR) * 32767.0f);
        if (mixL > 32767) mixL = 32767; else if (mixL < -32768) mixL = -32768;
        if (mixR > 32767) mixR = 32767; else if (mixR < -32768) mixR = -32768;

        int16_t out[2] = { static_cast<int16_t>(mixL), static_cast<int16_t>(mixR) };
        wavFile.write(reinterpret_cast<const char*>(out), sizeof(out));
        totalBytesWritten += sizeof(out);
    }

    if (wavFile.is_open()) {
        DWORD riffChunkSize = totalBytesWritten + 36;
        wavFile.seekp(4, std::ios::beg);
        wavFile.write(reinterpret_cast<const char*>(&riffChunkSize), 4);
        wavFile.seekp(40, std::ios::beg);
        wavFile.write(reinterpret_cast<const char*>(&totalBytesWritten), 4);
        wavFile.close();
    }

    if (pSysClient) { pSysClient->Stop(); pSysClient->Release(); }
    if (pSysCapture) pSysCapture->Release();
    if (pSilentClient) { pSilentClient->Stop(); pSilentClient->Release(); }
    if (pSilentRender) pSilentRender->Release();
    if (pSysDevice) pSysDevice->Release();
    if (pSysWfx) CoTaskMemFree(pSysWfx);

    if (pMicClient) { pMicClient->Stop(); pMicClient->Release(); }
    if (pMicCapture) pMicCapture->Release();
    if (pMicDevice) pMicDevice->Release();
    if (pMicWfx) CoTaskMemFree(pMicWfx);

    pEnumerator->Release();
    CoUninitialize();
}

// Переключение видеозаписи экрана
void ToggleVideoRecording() {
    if (g_isRecording) {
        // 1. Завершаем фоновый захват звука
        if (g_wasapiRecording.load()) {
            g_wasapiRecording = false;
            if (g_wasapiThread.joinable()) {
                g_wasapiThread.join();
            }
        }

        // 2. Посылаем 'q' в stdin пайп для корректного завершения записи видео и сохранения moov atom
        if (g_ffmpegProc.hProcess != NULL) {
            if (g_hFFmpegStdin != NULL) {
                DWORD written = 0;
                WriteFile(g_hFFmpegStdin, "q\n", 2, &written, NULL);
                FlushFileBuffers(g_hFFmpegStdin);
                CloseHandle(g_hFFmpegStdin);
                g_hFFmpegStdin = NULL;
            }
            DWORD waitRes = WaitForSingleObject(g_ffmpegProc.hProcess, 10000);
            if (waitRes == WAIT_TIMEOUT) {
                TerminateProcess(g_ffmpegProc.hProcess, 0);
            }
            CloseHandle(g_ffmpegProc.hProcess);
            CloseHandle(g_ffmpegProc.hThread);
            ZeroMemory(&g_ffmpegProc, sizeof(g_ffmpegProc));
        }

        // 3. Если требуется сведение звука (системного и/или микрофона) с видео
        if (g_isMuxingRequired) {
            std::wstring ffmpegPath = GetFFmpegPath();
            if (!ffmpegPath.empty() && 
                GetFileAttributesW(g_currentTempVideoPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
                GetFileAttributesW(g_currentTempAudioPath.c_str()) != INVALID_FILE_ATTRIBUTES) {

                // Флаг -shortest гарантирует, что контейнер завершится ровно тогда, когда закончится видео,
                // исключая любые зависания картинки при продолжающемся звуке
                std::wstring muxCmd = L"\"" + ffmpegPath + L"\" -y -i \"" + g_currentTempVideoPath +
                                      L"\" -i \"" + g_currentTempAudioPath +
                                      L"\" -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 192k -shortest -movflags +faststart \"" +
                                      g_currentFinalVideoPath + L"\"";

                STARTUPINFOW si = { sizeof(STARTUPINFOW) };
                si.dwFlags |= STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi = { 0 };

                std::vector<WCHAR> cmdBuf(muxCmd.begin(), muxCmd.end());
                cmdBuf.push_back(0);

                if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                    WaitForSingleObject(pi.hProcess, 30000);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }

                DeleteFileW(g_currentTempVideoPath.c_str());
                DeleteFileW(g_currentTempAudioPath.c_str());
            } else if (GetFileAttributesW(g_currentTempVideoPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                MoveFileW(g_currentTempVideoPath.c_str(), g_currentFinalVideoPath.c_str());
                DeleteFileW(g_currentTempAudioPath.c_str());
            }
            g_isMuxingRequired = false;
        }

        g_isRecording = false;
        ShowNotification(L"Видеозапись сохранена", L"Видео успешно сохранено в папку записей");
    } else {
        std::wstring ffmpegPath = GetFFmpegPath();
        if (ffmpegPath.empty()) {
            int ans = MessageBoxW(NULL,
                L"Для высокопроизводительной видеозаписи экрана требуется компонент FFmpeg.\n\nЗагрузить и установить его сейчас автоматически?",
                L"WinScreen — Запись видео экрана",
                MB_YESNO | MB_ICONQUESTION);
            if (ans == IDYES) {
                ShowNotification(L"Установка FFmpeg", L"Начата фоновая загрузка FFmpeg...");
                DownloadParams* dp = new DownloadParams{ NULL, NULL };
                CreateThread(NULL, 0, DownloadFFmpegThread, dp, 0, NULL);
            }
            return;
        }

        CreateDirectoryW(g_videoSaveDir.c_str(), NULL);
        time_t rawtime;
        struct tm* timeinfo;
        WCHAR timeBuf[64];
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        wcsftime(timeBuf, 64, L"%Y-%m-%d_%H-%M-%S", timeinfo);

        g_currentFinalVideoPath = g_videoSaveDir + L"\\Запись_" + timeBuf + L".mp4";

        bool needAudio = (g_recordSysAudio || g_recordMic);
        g_isMuxingRequired = needAudio;

        std::wstring videoTarget = needAudio ? (g_videoSaveDir + L"\\_ws_vtemp_" + timeBuf + L".mp4") : g_currentFinalVideoPath;
        if (needAudio) {
            g_currentTempVideoPath = videoTarget;
            g_currentTempAudioPath = g_videoSaveDir + L"\\_ws_atemp_" + timeBuf + L".wav";
        }

        // FFmpeg захватывает чистый рабочий стол без DirectShow-конфликтов
        std::wstring cmd = L"\"" + ffmpegPath + L"\" -y -f gdigrab -framerate 30 -i desktop -c:v libx264 -preset ultrafast -pix_fmt yuv420p -movflags +faststart \"" + videoTarget + L"\"";

        HANDLE hStdInRead = NULL;
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        if (CreatePipe(&hStdInRead, &g_hFFmpegStdin, &sa, 0)) {
            SetHandleInformation(g_hFFmpegStdin, HANDLE_FLAG_INHERIT, 0);
        } else {
            hStdInRead = NULL;
            g_hFFmpegStdin = NULL;
        }

        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        if (hStdInRead != NULL || (hNul != INVALID_HANDLE_VALUE)) {
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput = hStdInRead;
            si.hStdOutput = (hNul != INVALID_HANDLE_VALUE) ? hNul : NULL;
            si.hStdError = (hNul != INVALID_HANDLE_VALUE) ? hNul : NULL;
        }

        std::vector<WCHAR> cmdBuf(cmd.begin(), cmd.end());
        cmdBuf.push_back(0);

        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &g_ffmpegProc)) {
            g_isRecording = true;
            if (hStdInRead) CloseHandle(hStdInRead);
            if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

            // Запускаем нативный аудиопоток WASAPI одновременно с FFmpeg
            if (needAudio) {
                g_wasapiRecording = true;
                WasapiCaptureParams params;
                params.wavPath = g_currentTempAudioPath;
                params.recordSys = g_recordSysAudio;
                params.recordMic = g_recordMic;
                params.selectedMic = g_selectedMic;
                g_wasapiThread = std::thread(WasapiAudioCaptureThread, params);
            }

            ShowNotification(L"Запись видео начата", L"Идет запись экрана...\nНажмите " + g_hkRecord.displayText + L" для остановки");
        } else {
            if (hStdInRead) CloseHandle(hStdInRead);
            if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
            if (g_hFFmpegStdin) { CloseHandle(g_hFFmpegStdin); g_hFFmpegStdin = NULL; }
            g_isMuxingRequired = false;
            ShowNotification(L"Ошибка видеозаписи", L"Не удалось запустить процесс FFmpeg");
        }
    }
}

// Режимы выделения
void TriggerLiveAreaCapture() {
    std::thread([]() { StartOverlay(false); }).detach();
}

void TriggerFrozenAreaCapture() {
    std::thread([]() { StartOverlay(true); }).detach();
}

void CancelCurrentRecording() {
    if (g_recordingTarget == 1) {
        if (hEditHk1 && IsWindow(hEditHk1)) SetWindowTextW(hEditHk1, g_tempHkFull.displayText.c_str());
        if (hBtnHk1 && IsWindow(hBtnHk1)) SetWindowTextW(hBtnHk1, L"Задать...");
    } else if (g_recordingTarget == 2) {
        if (hEditHk2 && IsWindow(hEditHk2)) SetWindowTextW(hEditHk2, g_tempHkLive.displayText.c_str());
        if (hBtnHk2 && IsWindow(hBtnHk2)) SetWindowTextW(hBtnHk2, L"Задать...");
    } else if (g_recordingTarget == 3) {
        if (hEditHk3 && IsWindow(hEditHk3)) SetWindowTextW(hEditHk3, g_tempHkFrozen.displayText.c_str());
        if (hBtnHk3 && IsWindow(hBtnHk3)) SetWindowTextW(hBtnHk3, L"Задать...");
    } else if (g_recordingTarget == 4) {
        if (hEditHk4 && IsWindow(hEditHk4)) SetWindowTextW(hEditHk4, g_tempHkRecord.displayText.c_str());
        if (hBtnHk4 && IsWindow(hBtnHk4)) SetWindowTextW(hBtnHk4, L"Задать...");
    }
    g_recordingTarget = 0;
    s_recCtrl = s_recShift = s_recAlt = s_recWin = false;
}

void StartRecording(int target) {
    if (g_recordingTarget != 0) {
        CancelCurrentRecording();
    }
    g_recordingTarget = target;
    s_recCtrl = s_recShift = s_recAlt = s_recWin = false;
    HWND hEdit = (target == 1) ? hEditHk1 : (target == 2 ? hEditHk2 : (target == 3 ? hEditHk3 : hEditHk4));
    HWND hBtn  = (target == 1) ? hBtnHk1  : (target == 2 ? hBtnHk2  : (target == 3 ? hBtnHk3 : hBtnHk4));
    if (hEdit && IsWindow(hEdit)) SetWindowTextW(hEdit, L"[Нажмите клавиши...]");
    if (hBtn && IsWindow(hBtn)) SetWindowTextW(hBtn, L"Отмена");
    if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) SetFocus(g_hSettingsWnd);
}

// Низкоуровневый перехват клавиатуры (WH_KEYBOARD_LL)
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vk = pKey->vkCode;

        // 1. РЕЖИМ ЗАПИСИ ГОРЯЧЕЙ КЛАВИШИ В ОКНЕ НАСТРОЕК
        if (g_recordingTarget != 0) {
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                // Отмена записи по Escape
                if (vk == VK_ESCAPE) {
                    CancelCurrentRecording();
                    return 1;
                }

                // Проверяем нажатие клавиш-модификаторов
                bool isMod = false;
                if (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL) {
                    s_recCtrl = true;
                    isMod = true;
                }
                if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT) {
                    s_recShift = true;
                    isMod = true;
                }
                if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU) {
                    s_recAlt = true;
                    isMod = true;
                }
                if (vk == VK_LWIN || vk == VK_RWIN) {
                    s_recWin = true;
                    isMod = true;
                }
                if ((pKey->flags & LLKHF_ALTDOWN) != 0) {
                    s_recAlt = true;
                }

                if (isMod) {
                    // Моментальное интерактивное отображение зажатых модификаторов: "Alt + ...", "Ctrl + Alt + ..."
                    std::wstring preview;
                    if (s_recWin)   preview += L"Win + ";
                    if (s_recCtrl)  preview += L"Ctrl + ";
                    if (s_recShift) preview += L"Shift + ";
                    if (s_recAlt)   preview += L"Alt + ";
                    preview += L"...";

                    HWND hEdit = (g_recordingTarget == 1) ? hEditHk1 : (g_recordingTarget == 2 ? hEditHk2 : (g_recordingTarget == 3 ? hEditHk3 : hEditHk4));
                    if (hEdit && IsWindow(hEdit)) {
                        SetWindowTextW(hEdit, preview.c_str());
                    }
                    return 1; // Поглощаем событие нажатия модификатора
                } else {
                    // Нажата основная клавиша (PrintScreen, буква, цифра, F-клавиша и т.д.)
                    bool finalAlt   = s_recAlt || ((pKey->flags & LLKHF_ALTDOWN) != 0) || ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
                    bool finalCtrl  = s_recCtrl || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
                    bool finalShift = s_recShift || ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
                    bool finalWin   = s_recWin || ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

                    std::wstring text = FormatHotkey(vk, finalCtrl, finalShift, finalAlt, finalWin);

                    if (g_recordingTarget == 1) {
                        g_tempHkFull = { vk, finalCtrl, finalShift, finalAlt, finalWin, text };
                        if (hEditHk1 && IsWindow(hEditHk1)) SetWindowTextW(hEditHk1, text.c_str());
                        if (hBtnHk1 && IsWindow(hBtnHk1)) SetWindowTextW(hBtnHk1, L"Задать...");
                    } else if (g_recordingTarget == 2) {
                        g_tempHkLive = { vk, finalCtrl, finalShift, finalAlt, finalWin, text };
                        if (hEditHk2 && IsWindow(hEditHk2)) SetWindowTextW(hEditHk2, text.c_str());
                        if (hBtnHk2 && IsWindow(hBtnHk2)) SetWindowTextW(hBtnHk2, L"Задать...");
                    } else if (g_recordingTarget == 3) {
                        g_tempHkFrozen = { vk, finalCtrl, finalShift, finalAlt, finalWin, text };
                        if (hEditHk3 && IsWindow(hEditHk3)) SetWindowTextW(hEditHk3, text.c_str());
                        if (hBtnHk3 && IsWindow(hBtnHk3)) SetWindowTextW(hBtnHk3, L"Задать...");
                    } else if (g_recordingTarget == 4) {
                        g_tempHkRecord = { vk, finalCtrl, finalShift, finalAlt, finalWin, text };
                        if (hEditHk4 && IsWindow(hEditHk4)) SetWindowTextW(hEditHk4, text.c_str());
                        if (hBtnHk4 && IsWindow(hBtnHk4)) SetWindowTextW(hBtnHk4, L"Задать...");
                    }

                    g_recordingTarget = 0;
                    s_recCtrl = s_recShift = s_recAlt = s_recWin = false;
                    return 1; // Поглощаем клавишу
                }
            } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                // Пользователь отпустил модификатор до нажатия основной клавиши
                bool modChanged = false;
                if (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL) {
                    s_recCtrl = false;
                    modChanged = true;
                }
                if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT) {
                    s_recShift = false;
                    modChanged = true;
                }
                if (vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU) {
                    s_recAlt = false;
                    modChanged = true;
                }
                if (vk == VK_LWIN || vk == VK_RWIN) {
                    s_recWin = false;
                    modChanged = true;
                }

                if (modChanged) {
                    std::wstring preview;
                    if (s_recWin)   preview += L"Win + ";
                    if (s_recCtrl)  preview += L"Ctrl + ";
                    if (s_recShift) preview += L"Shift + ";
                    if (s_recAlt)   preview += L"Alt + ";

                    if (preview.empty()) {
                        preview = L"[Нажмите клавиши...]";
                    } else {
                        preview += L"...";
                    }

                    HWND hEdit = (g_recordingTarget == 1) ? hEditHk1 : (g_recordingTarget == 2 ? hEditHk2 : (g_recordingTarget == 3 ? hEditHk3 : hEditHk4));
                    if (hEdit && IsWindow(hEdit)) {
                        SetWindowTextW(hEdit, preview.c_str());
                    }
                }
                return 1;
            }
        }

        // 2. ОБЫЧНЫЙ ПЕРЕХВАТ ГОРЯЧИХ КЛАВИШ В ФОНЕ
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            bool alt   = ((pKey->flags & LLKHF_ALTDOWN) != 0) || ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
            bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool win   = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

            auto Matches = [](const HotkeyConfig& hk, DWORD k, bool c, bool s, bool a, bool w) {
                return (hk.vkCode == k && hk.ctrl == c && hk.shift == s && hk.alt == a && hk.win == w);
            };

            if (Matches(g_hkRecord, vk, ctrl, shift, alt, win)) {
                std::thread(ToggleVideoRecording).detach();
                return 1; // Поглощаем клавишу!
            } else if (Matches(g_hkFullscreen, vk, ctrl, shift, alt, win)) {
                std::thread(TriggerFullScreenCapture).detach();
                return 1; // Поглощаем клавишу для всей Windows!
            } else if (Matches(g_hkLive, vk, ctrl, shift, alt, win)) {
                TriggerLiveAreaCapture();
                return 1; // Поглощаем клавишу!
            } else if (Matches(g_hkFrozen, vk, ctrl, shift, alt, win)) {
                TriggerFrozenAreaCapture();
                return 1; // Поглощаем клавишу!
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// Автозагрузка в реестре Windows
bool IsAutostartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR val[MAX_PATH];
        DWORD size = sizeof(val);
        LSTATUS status = RegQueryValueExW(hKey, REG_APP_NAME, NULL, NULL, (LPBYTE)val, &size);
        RegCloseKey(hKey);
        return (status == ERROR_SUCCESS);
    }
    return false;
}

void SetAutostart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            WCHAR exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, REG_APP_NAME, 0, REG_SZ, (const BYTE*)cmd.c_str(), (cmd.length() + 1) * sizeof(WCHAR));
        } else {
            RegDeleteValueW(hKey, REG_APP_NAME);
        }
        RegCloseKey(hKey);
    }
}

// Отключение перехвата PrintScreen штатными Ножницами Windows 10/11
void DisableWindowsSnippingToolHook() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEYBOARD_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0; // 0 = отключить перехват Ножницами
        RegSetValueExW(hKey, REG_SNIP_KEY, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// Автоустановка при первом запуске
void AutoInstallIfNeeded() {
    DisableWindowsSnippingToolHook();

    WCHAR currentExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

    std::wstring appDir = GetAppDataDir();
    std::wstring targetExe = appDir + L"\\WinScreen.exe";

    // Если запущено не из %APPDATA%\WinScreen
    if (_wcsicmp(currentExe, targetExe.c_str()) != 0) {
        CopyFileW(currentExe, targetExe.c_str(), FALSE);

        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            std::wstring cmd = L"\"" + targetExe + L"\"";
            RegSetValueExW(hKey, REG_APP_NAME, 0, REG_SZ, (const BYTE*)cmd.c_str(), (cmd.length() + 1) * sizeof(WCHAR));
            RegCloseKey(hKey);
        }
    } else {
        if (!IsAutostartEnabled()) {
            SetAutostart(true);
        }
    }
}

void UninstallApp() {
    SetAutostart(false);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEYBOARD_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        RegSetValueExW(hKey, REG_SNIP_KEY, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }
    MessageBoxW(NULL, L"WinScreen успешно удалён из автозагрузки Windows.", L"WinScreen", MB_OK | MB_ICONINFORMATION);
    PostQuitMessage(0);
}

void OpenScreenshotsFolder() {
    CreateDirectoryW(g_saveDir.c_str(), NULL);
    ShellExecuteW(NULL, L"open", g_saveDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// Генерация значка трея в памяти
HICON CreateTrayIcon() {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, cx, cy);
    HGDIOBJ hOld = SelectObject(hMemDC, hBmp);

    // Рисуем синий квадрат с белой рамкой камеры
    RECT rc = { 0, 0, cx, cy };
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 120, 215));
    FillRect(hMemDC, &rc, hBrush);
    DeleteObject(hBrush);

    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    SelectObject(hMemDC, hPen);
    SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
    Rectangle(hMemDC, 3, 3, cx - 3, cy - 3);
    DeleteObject(hPen);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);

    ICONINFO ii = { TRUE, 0, 0, hBmp, hBmp };
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hBmp);
    return hIcon;
}

// Меню трея
void ShowTrayMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    HMENU hSubMenu = CreatePopupMenu();

    std::wstring m1 = L"1. Полный экран (" + g_hkFullscreen.displayText + L")";
    std::wstring m2 = L"2. Выделение живое (" + g_hkLive.displayText + L")";
    std::wstring m3 = L"3. Заморозка экрана (" + g_hkFrozen.displayText + L")";

    AppendMenuW(hSubMenu, MF_STRING, ID_TRAY_FULLSCREEN, m1.c_str());
    AppendMenuW(hSubMenu, MF_STRING, ID_TRAY_LIVE_AREA, m2.c_str());
    AppendMenuW(hSubMenu, MF_STRING, ID_TRAY_FROZEN_AREA, m3.c_str());

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, L"📷 Сделать скриншот");

    std::wstring recText = g_isRecording ? L"⏹ Остановить запись видео" : (L"🎥 Начать запись видео (" + g_hkRecord.displayText + L")");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_RECORD_VIDEO, recText.c_str());

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_FOLDER, L"📁 Открыть папку со скриншотами");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_VIDEO_DIR, L"🎬 Открыть папку с видеозаписями");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"⚙️ Настройки...");

    UINT autostartFlags = MF_STRING | (IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED);
    AppendMenuW(hMenu, autostartFlags, ID_TRAY_AUTOSTART, L"🔄 Автозагрузка с Windows");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"❌ Выход");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

// Окно настроек (нативный Win32 диалог)
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hChkPng = NULL;
    static HWND hChkAuto = NULL;
    static HWND hChkClip = NULL;
    static HWND hChkDisk = NULL;
    static HWND hChkNotif = NULL;
    static HWND hLblQuality = NULL;
    static HWND hSliderQuality = NULL;
    static HWND hEditQuality = NULL;
    static HWND hLblPercent = NULL;
    static HWND hBtnInstallFFmpeg = NULL;
    static HWND hChkSysAudio = NULL;
    static HWND hChkMic = NULL;
    static HWND hLblMicChoice = NULL;
    static HWND hComboMic = NULL;
    static std::vector<std::wstring> s_detectedMics;

    switch (msg) {
    case WM_HSCROLL: {
        if ((HWND)lParam == hSliderQuality) {
            int pos = (int)SendMessageW(hSliderQuality, TBM_GETPOS, 0, 0);
            if (pos < 1) pos = 1;
            if (pos > 100) pos = 100;
            g_jpgQuality = pos;
            std::wstring s = std::to_wstring(pos);
            SetWindowTextW(hEditQuality, s.c_str());
        }
        return 0;
    }
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // 1. Папка для скриншотов
        HWND lbl1 = CreateWindowW(L"STATIC", L"Папка для сохранения скриншотов:", WS_CHILD | WS_VISIBLE, 20, 10, 360, 16, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lbl1, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEditDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_saveDir.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 28, 375, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditDir, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnBrowse = CreateWindowW(L"BUTTON", L"Обзор...", WS_CHILD | WS_VISIBLE, 405, 28, 95, 23, hWnd, (HMENU)101, g_hInstance, NULL);
        SendMessageW(btnBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 2. Папка для видеозаписей
        HWND lblVid = CreateWindowW(L"STATIC", L"Папка для сохранения видеозаписей:", WS_CHILD | WS_VISIBLE, 20, 56, 290, 16, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblVid, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Кнопка установки FFmpeg (отображается ТОЛЬКО если FFmpeg не установлен в системе)
        if (!IsFFmpegInstalled()) {
            hBtnInstallFFmpeg = CreateWindowW(L"BUTTON", L"📥 Установить FFmpeg", WS_CHILD | WS_VISIBLE, 320, 50, 180, 22, hWnd, (HMENU)105, g_hInstance, NULL);
            SendMessageW(hBtnInstallFFmpeg, WM_SETFONT, (WPARAM)hFont, TRUE);
        } else {
            hBtnInstallFFmpeg = NULL;
        }

        hEditVideoDir = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_videoSaveDir.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 74, 375, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditVideoDir, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnBrowseVid = CreateWindowW(L"BUTTON", L"Обзор...", WS_CHILD | WS_VISIBLE, 405, 74, 95, 23, hWnd, (HMENU)104, g_hInstance, NULL);
        SendMessageW(btnBrowseVid, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 3. Горячие клавиши
        HWND lblKeys = CreateWindowW(L"STATIC", L"Горячие клавиши (нажмите «Задать...» и наберите комбинацию на клавиатуре):", WS_CHILD | WS_VISIBLE, 20, 104, 480, 16, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblKeys, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Строка 1: Полный экран
        HWND lblHk1 = CreateWindowW(L"STATIC", L"1. Полный экран:", WS_CHILD | WS_VISIBLE, 20, 126, 160, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblHk1, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEditHk1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_tempHkFull.displayText.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 185, 124, 210, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditHk1, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnHk1 = CreateWindowW(L"BUTTON", L"Задать...", WS_CHILD | WS_VISIBLE, 405, 124, 95, 23, hWnd, (HMENU)201, g_hInstance, NULL);
        SendMessageW(hBtnHk1, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Строка 2: Выделение живое
        HWND lblHk2 = CreateWindowW(L"STATIC", L"2. Выделение (живое):", WS_CHILD | WS_VISIBLE, 20, 153, 160, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblHk2, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEditHk2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_tempHkLive.displayText.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 185, 151, 210, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditHk2, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnHk2 = CreateWindowW(L"BUTTON", L"Задать...", WS_CHILD | WS_VISIBLE, 405, 151, 95, 23, hWnd, (HMENU)202, g_hInstance, NULL);
        SendMessageW(hBtnHk2, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Строка 3: Заморозка экрана
        HWND lblHk3 = CreateWindowW(L"STATIC", L"3. Заморозка экрана:", WS_CHILD | WS_VISIBLE, 20, 180, 160, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblHk3, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEditHk3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_tempHkFrozen.displayText.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 185, 178, 210, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditHk3, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnHk3 = CreateWindowW(L"BUTTON", L"Задать...", WS_CHILD | WS_VISIBLE, 405, 178, 95, 23, hWnd, (HMENU)203, g_hInstance, NULL);
        SendMessageW(hBtnHk3, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Строка 4: Видеозапись экрана
        HWND lblHk4 = CreateWindowW(L"STATIC", L"4. Видеозапись экрана:", WS_CHILD | WS_VISIBLE, 20, 207, 160, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblHk4, WM_SETFONT, (WPARAM)hFont, TRUE);

        hEditHk4 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_tempHkRecord.displayText.c_str(), WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 185, 205, 210, 23, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hEditHk4, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnHk4 = CreateWindowW(L"BUTTON", L"Задать...", WS_CHILD | WS_VISIBLE, 405, 205, 95, 23, hWnd, (HMENU)204, g_hInstance, NULL);
        SendMessageW(hBtnHk4, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 4. Формат и качество
        hChkPng = CreateWindowW(L"BUTTON", L"Формат PNG (максимальное качество без потерь)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 236, 440, 20, hWnd, (HMENU)106, g_hInstance, NULL);
        SendMessageW(hChkPng, WM_SETFONT, (WPARAM)hFont, TRUE);
        bool isPng = (g_fileFormat == L"png");
        SendMessageW(hChkPng, BM_SETCHECK, isPng ? BST_CHECKED : BST_UNCHECKED, 0);

        hLblQuality = CreateWindowW(L"STATIC", L"Качество JPG:", WS_CHILD | (isPng ? 0 : WS_VISIBLE), 20, 260, 95, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblQuality, WM_SETFONT, (WPARAM)hFont, TRUE);

        hSliderQuality = CreateWindowExW(0, TRACKBAR_CLASSW, L"QualitySlider",
            WS_CHILD | (isPng ? 0 : WS_VISIBLE) | TBS_HORZ | TBS_AUTOTICKS,
            115, 256, 295, 28, hWnd, (HMENU)110, g_hInstance, NULL);
        SendMessageW(hSliderQuality, TBM_SETRANGE, TRUE, MAKELPARAM(1, 100));
        SendMessageW(hSliderQuality, TBM_SETPOS, TRUE, g_jpgQuality);
        SendMessageW(hSliderQuality, TBM_SETTICFREQ, 10, 0);

        std::wstring qStr = std::to_wstring(g_jpgQuality);
        hEditQuality = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", qStr.c_str(),
            WS_CHILD | (isPng ? 0 : WS_VISIBLE) | ES_NUMBER, 415, 258, 50, 22, hWnd, (HMENU)111, g_hInstance, NULL);
        SendMessageW(hEditQuality, WM_SETFONT, (WPARAM)hFont, TRUE);

        hLblPercent = CreateWindowW(L"STATIC", L"%", WS_CHILD | (isPng ? 0 : WS_VISIBLE), 470, 260, 20, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblPercent, WM_SETFONT, (WPARAM)hFont, TRUE);

        // 5. Звук при видеозаписи
        AudioDevices audioDevs = EnumerateAudioDevices();
        s_detectedMics = audioDevs.microphones;

        hChkSysAudio = CreateWindowW(L"BUTTON", L"Записывать звук системы", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 286, 440, 20, hWnd, (HMENU)107, g_hInstance, NULL);
        SendMessageW(hChkSysAudio, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkSysAudio, BM_SETCHECK, g_recordSysAudio ? BST_CHECKED : BST_UNCHECKED, 0);

        std::wstring micTitle = L"Записывать микрофон";
        bool micAvailable = !s_detectedMics.empty();
        if (!micAvailable) {
            micTitle = L"Записывать микрофон (микрофон не обнаружен)";
        } else if (s_detectedMics.size() == 1) {
            micTitle = L"Записывать микрофон (" + s_detectedMics[0] + L")";
        }

        hChkMic = CreateWindowW(L"BUTTON", micTitle.c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 308, 440, 20, hWnd, (HMENU)108, g_hInstance, NULL);
        SendMessageW(hChkMic, WM_SETFONT, (WPARAM)hFont, TRUE);
        if (!micAvailable) {
            EnableWindow(hChkMic, FALSE);
            SendMessageW(hChkMic, BM_SETCHECK, BST_UNCHECKED, 0);
        } else {
            SendMessageW(hChkMic, BM_SETCHECK, g_recordMic ? BST_CHECKED : BST_UNCHECKED, 0);
        }

        bool showCombo = (s_detectedMics.size() > 1 && g_recordMic);
        hLblMicChoice = CreateWindowW(L"STATIC", L"Микрофон:", WS_CHILD | (showCombo ? WS_VISIBLE : 0), 40, 332, 80, 16, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hLblMicChoice, WM_SETFONT, (WPARAM)hFont, TRUE);

        hComboMic = CreateWindowExW(WS_EX_CLIENTEDGE, L"COMBOBOX", L"", WS_CHILD | (showCombo ? WS_VISIBLE : 0) | CBS_DROPDOWNLIST | WS_VSCROLL, 125, 330, 375, 160, hWnd, (HMENU)109, g_hInstance, NULL);
        SendMessageW(hComboMic, WM_SETFONT, (WPARAM)hFont, TRUE);

        int selectIdx = 0;
        for (size_t i = 0; i < s_detectedMics.size(); ++i) {
            SendMessageW(hComboMic, CB_ADDSTRING, 0, (LPARAM)s_detectedMics[i].c_str());
            if (s_detectedMics[i] == g_selectedMic) {
                selectIdx = (int)i;
            }
        }
        if (!s_detectedMics.empty()) {
            SendMessageW(hComboMic, CB_SETCURSEL, selectIdx, 0);
        }

        // 6. Дополнительные опции
        hChkClip = CreateWindowW(L"BUTTON", L"Копировать скриншот в буфер обмена", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 360, 440, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hChkClip, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkClip, BM_SETCHECK, g_copyClipboard ? BST_CHECKED : BST_UNCHECKED, 0);

        hChkDisk = CreateWindowW(L"BUTTON", L"Сохранять скриншот на диск в папку", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 382, 440, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hChkDisk, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkDisk, BM_SETCHECK, g_saveDisk ? BST_CHECKED : BST_UNCHECKED, 0);

        hChkNotif = CreateWindowW(L"BUTTON", L"Показывать всплывающие уведомления Windows", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 404, 440, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hChkNotif, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkNotif, BM_SETCHECK, g_showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);

        hChkAuto = CreateWindowW(L"BUTTON", L"Запускать автоматически вместе с Windows", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 20, 426, 440, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(hChkAuto, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hChkAuto, BM_SETCHECK, IsAutostartEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

        // 7. Кнопки сохранения и удаления
        HWND btnDel = CreateWindowW(L"BUTTON", L"Удалить из системы", WS_CHILD | WS_VISIBLE, 20, 465, 160, 28, hWnd, (HMENU)103, g_hInstance, NULL);
        SendMessageW(btnDel, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnSave = CreateWindowW(L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 385, 465, 115, 28, hWnd, (HMENU)102, g_hInstance, NULL);
        SendMessageW(btnSave, WM_SETFONT, (WPARAM)hFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 111 && HIWORD(wParam) == EN_CHANGE) {
            WCHAR buf[16];
            GetWindowTextW(hEditQuality, buf, 16);
            int val = _wtoi(buf);
            if (val >= 1 && val <= 100) {
                g_jpgQuality = val;
                if (hSliderQuality && IsWindow(hSliderQuality)) {
                    SendMessageW(hSliderQuality, TBM_SETPOS, TRUE, val);
                }
            }
        } else if (id == 106) { // Переключение формата PNG / JPG
            bool isPng = (SendMessageW(hChkPng, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int showCmd = isPng ? SW_HIDE : SW_SHOW;
            if (hLblQuality) ShowWindow(hLblQuality, showCmd);
            if (hSliderQuality) ShowWindow(hSliderQuality, showCmd);
            if (hEditQuality) ShowWindow(hEditQuality, showCmd);
            if (hLblPercent) ShowWindow(hLblPercent, showCmd);
        } else if (id == 108) { // Переключение микрофона
            bool isChecked = (SendMessageW(hChkMic, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (s_detectedMics.size() > 1) {
                if (hLblMicChoice) ShowWindow(hLblMicChoice, isChecked ? SW_SHOW : SW_HIDE);
                if (hComboMic) ShowWindow(hComboMic, isChecked ? SW_SHOW : SW_HIDE);
            }
        } else if (id == 105) { // Установка FFmpeg
            if (hBtnInstallFFmpeg && IsWindow(hBtnInstallFFmpeg)) {
                EnableWindow(hBtnInstallFFmpeg, FALSE);
                SetWindowTextW(hBtnInstallFFmpeg, L"⏳ Загрузка FFmpeg...");
                DownloadParams* dp = new DownloadParams{ hWnd, hBtnInstallFFmpeg };
                CreateThread(NULL, 0, DownloadFFmpegThread, dp, 0, NULL);
            }
        } else if (id == 101) { // Обзор папки скриншотов
            WCHAR path[MAX_PATH];
            BROWSEINFOW bi = { 0 };
            bi.lpszTitle = L"Выберите папку для сохранения скриншотов:";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl && SHGetPathFromIDListW(pidl, path)) {
                SetWindowTextW(hEditDir, path);
            }
        } else if (id == 104) { // Обзор папки видеозаписей
            WCHAR path[MAX_PATH];
            BROWSEINFOW bi = { 0 };
            bi.lpszTitle = L"Выберите папку для сохранения видеозаписей:";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl && SHGetPathFromIDListW(pidl, path)) {
                SetWindowTextW(hEditVideoDir, path);
            }
        } else if (id == 201) { // Задать Hotkey 1
            if (g_recordingTarget == 1) {
                CancelCurrentRecording();
            } else {
                StartRecording(1);
            }
        } else if (id == 202) { // Задать Hotkey 2
            if (g_recordingTarget == 2) {
                CancelCurrentRecording();
            } else {
                StartRecording(2);
            }
        } else if (id == 203) { // Задать Hotkey 3
            if (g_recordingTarget == 3) {
                CancelCurrentRecording();
            } else {
                StartRecording(3);
            }
        } else if (id == 204) { // Задать Hotkey 4 (Видеозапись)
            if (g_recordingTarget == 4) {
                CancelCurrentRecording();
            } else {
                StartRecording(4);
            }
        } else if (id == 102) { // Сохранить
            CancelCurrentRecording();

            WCHAR buf[MAX_PATH];
            GetWindowTextW(hEditDir, buf, MAX_PATH);
            g_saveDir = buf;

            GetWindowTextW(hEditVideoDir, buf, MAX_PATH);
            g_videoSaveDir = buf;

            g_fileFormat = (SendMessageW(hChkPng, BM_GETCHECK, 0, 0) == BST_CHECKED) ? L"png" : L"jpg";

            WCHAR qBuf[16];
            GetWindowTextW(hEditQuality, qBuf, 16);
            int qVal = _wtoi(qBuf);
            if (qVal < 1 || qVal > 100) {
                if (hSliderQuality && IsWindow(hSliderQuality)) {
                    qVal = (int)SendMessageW(hSliderQuality, TBM_GETPOS, 0, 0);
                }
            }
            g_jpgQuality = max(1, min(100, qVal > 0 ? qVal : 90));

            g_recordSysAudio = (SendMessageW(hChkSysAudio, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_recordMic = (SendMessageW(hChkMic, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (hComboMic && s_detectedMics.size() > 1) {
                int sel = (int)SendMessageW(hComboMic, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)s_detectedMics.size()) {
                    g_selectedMic = s_detectedMics[sel];
                }
            } else if (s_detectedMics.size() == 1) {
                g_selectedMic = s_detectedMics[0];
            }

            g_copyClipboard = (SendMessageW(hChkClip, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_saveDisk = (SendMessageW(hChkDisk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_showNotifications = (SendMessageW(hChkNotif, BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool wantAuto = (SendMessageW(hChkAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetAutostart(wantAuto);

            // Применяем горячие клавиши
            g_hkFullscreen = g_tempHkFull;
            g_hkLive       = g_tempHkLive;
            g_hkFrozen     = g_tempHkFrozen;
            g_hkRecord     = g_tempHkRecord;

            SaveSettings();
            MessageBoxW(hWnd, L"Настройки успешно сохранены!", L"WinScreen", MB_OK | MB_ICONINFORMATION);
            DestroyWindow(hWnd);
        } else if (id == 103) { // Удалить
            if (MessageBoxW(hWnd, L"Вы уверены, что хотите удалить WinScreen из автозагрузки?", L"Удаление WinScreen", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                UninstallApp();
            }
        }
        return 0;
    }
    case WM_CLOSE:
        CancelCurrentRecording();
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        CancelCurrentRecording();
        g_hSettingsWnd = NULL;
        hEditHk1 = hBtnHk1 = NULL;
        hEditHk2 = hBtnHk2 = NULL;
        hEditHk3 = hBtnHk3 = NULL;
        hEditHk4 = hBtnHk4 = NULL;
        hEditDir = hEditVideoDir = NULL;
        hLblQuality = hSliderQuality = hEditQuality = hLblPercent = NULL;
        hBtnInstallFFmpeg = NULL;
        hChkSysAudio = hChkMic = hLblMicChoice = hComboMic = NULL;
        s_detectedMics.clear();
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void OpenSettingsDialog() {
    if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) {
        SetForegroundWindow(g_hSettingsWnd);
        return;
    }

    // Гарантируем, что хук клавиатуры активен для записи клавиш
    if (!g_hKeyboardHook) {
        g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInstance, 0);
    }

    g_recordingTarget = 0;
    s_recCtrl = s_recShift = s_recAlt = s_recWin = false;

    // Инициализируем временные конфигурации текущими значениями
    g_tempHkFull   = g_hkFullscreen;
    g_tempHkLive   = g_hkLive;
    g_tempHkFrozen = g_hkFrozen;
    g_tempHkRecord = g_hkRecord;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"WinScreen_Settings";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    int w = 535, h = 550;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hSettingsWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        wc.lpszClassName,
        L"WinScreen — Настройки",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        sx, sy, w, h,
        NULL, NULL, g_hInstance, NULL
    );
    SetForegroundWindow(g_hSettingsWnd);
}

// Главная процедура скрытого окна
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowTrayMenu(hWnd);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case ID_TRAY_FULLSCREEN:
            TriggerFullScreenCapture();
            break;
        case ID_TRAY_LIVE_AREA:
            TriggerLiveAreaCapture();
            break;
        case ID_TRAY_FROZEN_AREA:
            TriggerFrozenAreaCapture();
            break;
        case ID_TRAY_RECORD_VIDEO:
            std::thread(ToggleVideoRecording).detach();
            break;
        case ID_TRAY_OPEN_FOLDER:
            OpenScreenshotsFolder();
            break;
        case ID_TRAY_OPEN_VIDEO_DIR:
            ShellExecuteW(NULL, L"open", g_videoSaveDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        case ID_TRAY_SETTINGS:
            OpenSettingsDialog();
            break;
        case ID_TRAY_AUTOSTART:
            SetAutostart(!IsAutostartEnabled());
            break;
        case ID_TRAY_EXIT:
            if (g_isRecording) {
                ToggleVideoRecording();
            }
            PostQuitMessage(0);
            break;
        }
        return 0;
    }
    case WM_DESTROY: {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Главная функция WinMain
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int) {
    g_hInstance = hInstance;

    // Инициализация общих элементов управления Windows (Trackbar и др.)
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    // Инициализация GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    LoadSettings();

    // Обработка CLI команд
    std::wstring cmd = pCmdLine ? pCmdLine : L"";
    if (cmd == L"--uninstall") {
        UninstallApp();
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    } else if (cmd == L"--settings") {
        OpenSettingsDialog();
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    } else if (cmd == L"--help" || cmd == L"-h") {
        MessageBoxW(NULL,
            L"WinScreen — Параметры командной строки:\n\n"
            L"  --settings       Открыть графическое окно настроек\n"
            L"  --status         Показать текущую конфигурацию и горячие клавиши\n"
            L"  --uninstall      Удалить из автозагрузки и системы\n\n"
            L"Запуск без параметров автоматически активирует фоновую службу,\n"
            L"перехват горячих клавиш и сворачивает программу в системный трей.",
            L"WinScreen — Справка", MB_OK | MB_ICONINFORMATION);
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    } else if (cmd == L"--status") {
        std::wstring status = L"Текущая конфигурация WinScreen:\n\n"
            L"• Папка скриншотов: " + g_saveDir + L"\n"
            L"• Папка видеозаписей: " + g_videoSaveDir + L"\n"
            L"• Формат фото: " + g_fileFormat + L"\n"
            L"• Качество фото (JPG): " + std::to_wstring(g_jpgQuality) + L"%\n"
            L"• Звук системы: " + (g_recordSysAudio ? L"Включен" : L"Выключен") + L"\n"
            L"• Микрофон: " + (g_recordMic ? L"Включен" : L"Выключен") + L"\n"
            L"• Выбранный микрофон: " + (g_selectedMic.empty() ? L"(по умолчанию)" : g_selectedMic) + L"\n"
            L"• Буфер обмена: " + (g_copyClipboard ? L"Включен" : L"Выключен") + L"\n"
            L"• Сохранение на диск: " + (g_saveDisk ? L"Включено" : L"Выключено") + L"\n"
            L"• Автозагрузка Windows: " + (IsAutostartEnabled() ? L"Включена" : L"Выключена") + L"\n\n"
            L"Горячие клавиши:\n"
            L"1. Полный экран: " + g_hkFullscreen.displayText + L"\n"
            L"2. Выделение (живое): " + g_hkLive.displayText + L"\n"
            L"3. Заморозка экрана: " + g_hkFrozen.displayText + L"\n"
            L"4. Видеозапись: " + g_hkRecord.displayText;
        MessageBoxW(NULL, status.c_str(), L"WinScreen — Статус", MB_OK | MB_ICONINFORMATION);
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    // Автоматическая установка при первом старте
    AutoInstallIfNeeded();

    // Создание скрытого служебного окна
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WinScreen_Core";
    RegisterClassExW(&wc);

    g_hMainWnd = CreateWindowExW(
        0, wc.lpszClassName, L"WinScreen Core",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL
    );

    // Добавление иконки в трей
    g_nid.hWnd = g_hMainWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = CreateTrayIcon();
    wcsncpy(g_nid.szTip, L"WinScreen — Система скриншотов", sizeof(g_nid.szTip) / sizeof(WCHAR) - 1);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Установка низкоуровневого хука клавиатуры (WH_KEYBOARD_LL)
    g_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);

    // Основной цикл сообщений
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }

    if (g_nid.hIcon) {
        DestroyIcon(g_nid.hIcon);
    }

    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
