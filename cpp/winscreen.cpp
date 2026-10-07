#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <urlmon.h>
#include <wininet.h>
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
#include <tlhelp32.h>
#include "json.hpp"

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
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "version.lib")

using namespace Gdiplus;

// Версия и ссылки
const std::wstring APP_VERSION = L"1.2.0";
const std::wstring GITHUB_REPO_URL = L"https://github.com/Hikerys/WinScreen";
const std::wstring GITHUB_API_LATEST_RELEASE = L"https://api.github.com/repos/Hikerys/WinScreen/releases/latest";

// Константы приложения
#define WM_TRAYICON (WM_USER + 1)
#define WM_UPDATE_CHECK_DONE (WM_USER + 50)
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
    bool isMouseDown = false;
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

    // Подсветка и захват окна под курсором
    HWND hoveredWnd = NULL;
    RECT hoveredRect = { 0, 0, 0, 0 }; // в координатах оверлея
    POINT lastMouseMovePt = { -1, -1 };
} g_sel;

// Прототипы
void TriggerFullScreenCapture();
void TriggerLiveAreaCapture();
void TriggerFrozenAreaCapture();
void ShowNotification(const std::wstring& title, const std::wstring& message);
void LoadSettings();
void SaveSettings();
bool IsAutostartEnabled();
void SetAutostart(bool enable);
void UninstallApp();
void OpenScreenshotsFolder();
void OpenSettingsDialog();
void KillPreviousInstances();
bool ShowInstallerDialog(HINSTANCE hInstance, bool& outAutostart);
bool PerformInstallOrUpgrade(const std::wstring& currentExe, bool enableAutostart);
std::wstring GetInstalledVersion();
int CompareVersions(const std::wstring& v1, const std::wstring& v2);

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

std::wstring GetSettingsFilePath() {
    return GetAppDataDir() + L"\\settings.json";
}

std::wstring GetLegacyConfigFilePath() {
    return GetAppDataDir() + L"\\config.txt";
}

std::wstring GetLegacyJsonConfigFilePath() {
    return GetAppDataDir() + L"\\config.json";
}

std::wstring GetInstalledExePath() {
    return GetAppDataDir() + L"\\WinScreen.exe";
}

bool IsWinScreenInstalled() {
    std::wstring p = GetInstalledExePath();
    DWORD attr = GetFileAttributesW(p.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

// Преобразование UTF-8 <-> Wide String
std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), NULL, 0);
    std::wstring wide(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.length(), &wide[0], len);
    return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), NULL, 0, NULL, NULL);
    std::string utf8(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.length(), &utf8[0], len, NULL, NULL);
    return utf8;
}

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

SemVer ParseSemVer(const std::wstring& s) {
    SemVer v;
    std::wstring clean = s;
    if (!clean.empty() && (clean[0] == L'v' || clean[0] == L'V')) {
        clean = clean.substr(1);
    }
    std::wstringstream ss(clean);
    std::wstring part;
    if (std::getline(ss, part, L'.')) v.major = _wtoi(part.c_str());
    if (std::getline(ss, part, L'.')) v.minor = _wtoi(part.c_str());
    if (std::getline(ss, part, L'.')) v.patch = _wtoi(part.c_str());
    return v;
}

int CompareVersions(const std::wstring& v1, const std::wstring& v2) {
    SemVer a = ParseSemVer(v1);
    SemVer b = ParseSemVer(v2);
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

std::wstring GetExeFileVersion(const std::wstring& filePath) {
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(filePath.c_str(), &handle);
    if (size == 0) return L"";
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(filePath.c_str(), handle, size, data.data())) return L"";

    VS_FIXEDFILEINFO* pFileInfo = NULL;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\", (LPVOID*)&pFileInfo, &len) && len >= sizeof(VS_FIXEDFILEINFO)) {
        int maj = HIWORD(pFileInfo->dwFileVersionMS);
        int min = LOWORD(pFileInfo->dwFileVersionMS);
        int patch = HIWORD(pFileInfo->dwFileVersionLS);
        return std::to_wstring(maj) + L"." + std::to_wstring(min) + L"." + std::to_wstring(patch);
    }
    return L"";
}

std::wstring GetInstalledVersion() {
    if (!IsWinScreenInstalled()) return L"";

    // 1. Проверяем settings.json
    std::wstring jsonPath = GetSettingsFilePath();
    std::ifstream fin(WideToUtf8(jsonPath), std::ios::binary);
    if (fin.is_open()) {
        std::stringstream ss;
        ss << fin.rdbuf();
        fin.close();
        std::string s = ss.str();
        if (!s.empty()) {
            JsonParser p(Utf8ToWide(s));
            JsonValue root = p.parseValue();
            if (root.isObject() && root.has(L"version")) {
                std::wstring v = root.getString(L"version");
                if (!v.empty()) return v;
            }
        }
    }

    // 2. Проверяем PE FileVersion установленного exe
    std::wstring peVer = GetExeFileVersion(GetInstalledExePath());
    if (!peVer.empty()) return peVer;

    // 3. Если файл существует, но версии нет (v1.1.0/v1.0.0)
    return L"1.1.0";
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

class DownloadProgressCallback : public IBindStatusCallback {
public:
    HWND m_hBtn;
    bool* m_pCancel;

    DownloadProgressCallback(HWND hBtn = NULL, bool* pCancel = NULL)
        : m_hBtn(hBtn), m_pCancel(pCancel) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) {
        if (!ppvObject) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IBindStatusCallback) {
            *ppvObject = static_cast<IBindStatusCallback*>(this);
            return S_OK;
        }
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return 1; }
    STDMETHOD_(ULONG, Release)() { return 1; }

    STDMETHOD(OnStartBinding)(DWORD, IBinding*) { return S_OK; }
    STDMETHOD(GetPriority)(LONG*) { return S_OK; }
    STDMETHOD(OnLowResource)(DWORD) { return S_OK; }
    STDMETHOD(OnProgress)(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText) {
        if (m_pCancel && *m_pCancel) return E_ABORT;
        if (ulProgressMax > 0 && m_hBtn && IsWindow(m_hBtn)) {
            int pct = (int)((unsigned long long)ulProgress * 100 / ulProgressMax);
            std::wstring txt = L"⏳ Загрузка FFmpeg (" + std::to_wstring(pct) + L"%)...";
            SetWindowTextW(m_hBtn, txt.c_str());
        }
        return S_OK;
    }
    STDMETHOD(OnStopBinding)(HRESULT, LPCWSTR) { return S_OK; }
    STDMETHOD(GetBindInfo)(DWORD*, BINDINFO*) { return S_OK; }
    STDMETHOD(OnDataAvailable)(DWORD, DWORD, FORMATETC*, STGMEDIUM*) { return S_OK; }
    STDMETHOD(OnObjectAvailable)(REFIID, IUnknown*) { return S_OK; }
};

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

    DownloadProgressCallback cb(dp ? dp->hBtn : NULL, NULL);
    HRESULT hr = URLDownloadToFileW(NULL, url, tempDest.c_str(), 0, &cb);
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

void SaveHotkeyToJson(JsonValue& obj, const std::wstring& name, const HotkeyConfig& hk) {
    JsonValue hkObj = JsonValue::Object();
    hkObj.set(L"vk", (int)hk.vkCode);
    hkObj.set(L"ctrl", hk.ctrl);
    hkObj.set(L"shift", hk.shift);
    hkObj.set(L"alt", hk.alt);
    hkObj.set(L"win", hk.win);
    hkObj.set(L"display", hk.displayText);
    obj.set(name, hkObj);
}

void LoadHotkeyFromJson(const JsonValue& obj, const std::wstring& name, HotkeyConfig& hk) {
    if (!obj.has(name)) return;
    JsonValue hkObj = obj.getObj(name);
    hk.vkCode = (DWORD)hkObj.getInt(L"vk", (int)hk.vkCode);
    hk.ctrl = hkObj.getBool(L"ctrl", hk.ctrl);
    hk.shift = hkObj.getBool(L"shift", hk.shift);
    hk.alt = hkObj.getBool(L"alt", hk.alt);
    hk.win = hkObj.getBool(L"win", hk.win);
    hk.displayText = FormatHotkey(hk.vkCode, hk.ctrl, hk.shift, hk.alt, hk.win);
}

// Загрузка и сохранение настроек
void LoadSettings() {
    g_saveDir = GetDefaultScreenshotsDir();
    g_videoSaveDir = GetDefaultRecordingsDir();

    std::wstring jsonPath = GetSettingsFilePath();
    std::wstring legacyJsonPath = GetLegacyJsonConfigFilePath();
    std::wstring legacyPath = GetLegacyConfigFilePath();

    bool loaded = false;

    // 1. Попытка загрузить современный settings.json
    std::ifstream fin(WideToUtf8(jsonPath), std::ios::binary);
    if (!fin.is_open()) {
        // Проверяем config.json (от Python версии)
        fin.open(WideToUtf8(legacyJsonPath), std::ios::binary);
    }

    if (fin.is_open()) {
        std::stringstream ss;
        ss << fin.rdbuf();
        fin.close();
        std::string content = ss.str();
        if (!content.empty()) {
            JsonParser parser(Utf8ToWide(content));
            JsonValue root = parser.parseValue();
            if (root.isObject()) {
                loaded = true;
                if (root.has(L"save_dir")) g_saveDir = root.getString(L"save_dir");
                else if (root.has(L"save_directory")) g_saveDir = root.getString(L"save_directory");

                if (root.has(L"video_dir")) g_videoSaveDir = root.getString(L"video_dir");
                else if (root.has(L"video_save_directory")) g_videoSaveDir = root.getString(L"video_save_directory");

                if (root.has(L"format")) g_fileFormat = root.getString(L"format");
                else if (root.has(L"file_format")) g_fileFormat = root.getString(L"file_format");

                if (root.has(L"quality")) g_jpgQuality = max(1, min(100, root.getInt(L"quality", 90)));
                else if (root.has(L"jpg_quality")) g_jpgQuality = max(1, min(100, root.getInt(L"jpg_quality", 90)));

                if (root.has(L"copy_clipboard")) g_copyClipboard = root.getBool(L"copy_clipboard", true);
                else if (root.has(L"copy_to_clipboard")) g_copyClipboard = root.getBool(L"copy_to_clipboard", true);

                if (root.has(L"save_disk")) g_saveDisk = root.getBool(L"save_disk", true);
                else if (root.has(L"save_to_disk")) g_saveDisk = root.getBool(L"save_to_disk", true);

                if (root.has(L"notifications")) g_showNotifications = root.getBool(L"notifications", true);
                else if (root.has(L"show_notifications")) g_showNotifications = root.getBool(L"show_notifications", true);

                if (root.has(L"record_system_audio")) g_recordSysAudio = root.getBool(L"record_system_audio", false);
                if (root.has(L"record_microphone")) g_recordMic = root.getBool(L"record_microphone", false);
                if (root.has(L"selected_microphone")) g_selectedMic = root.getString(L"selected_microphone", L"");

                if (root.has(L"hotkeys")) {
                    JsonValue hks = root.getObj(L"hotkeys");
                    LoadHotkeyFromJson(hks, L"fullscreen", g_hkFullscreen);
                    LoadHotkeyFromJson(hks, L"live", g_hkLive);
                    LoadHotkeyFromJson(hks, L"frozen", g_hkFrozen);
                    LoadHotkeyFromJson(hks, L"record", g_hkRecord);
                }
            }
        }
    }

    // 2. Если JSON не загружен, проверяем legacy config.txt (от v1.1.0)
    if (!loaded) {
        std::wifstream lfin(legacyPath);
        if (lfin.is_open()) {
            std::wstring line;
            while (std::getline(lfin, line)) {
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
            lfin.close();
            // Сразу сохраняем прочитанные настройки в новый формат settings.json
            SaveSettings();
        }
    }

    CreateDirectoryW(g_saveDir.c_str(), NULL);
    CreateDirectoryW(g_videoSaveDir.c_str(), NULL);
}

void SaveSettings() {
    CreateDirectoryW(GetAppDataDir().c_str(), NULL);

    JsonValue root = JsonValue::Object();
    root.set(L"version", APP_VERSION);
    root.set(L"save_dir", g_saveDir);
    root.set(L"video_dir", g_videoSaveDir);
    root.set(L"format", g_fileFormat);
    root.set(L"quality", g_jpgQuality);
    root.set(L"copy_clipboard", g_copyClipboard);
    root.set(L"save_disk", g_saveDisk);
    root.set(L"notifications", g_showNotifications);
    root.set(L"autostart", IsAutostartEnabled());
    root.set(L"record_system_audio", g_recordSysAudio);
    root.set(L"record_microphone", g_recordMic);
    root.set(L"selected_microphone", g_selectedMic);

    JsonValue hks = JsonValue::Object();
    SaveHotkeyToJson(hks, L"fullscreen", g_hkFullscreen);
    SaveHotkeyToJson(hks, L"live", g_hkLive);
    SaveHotkeyToJson(hks, L"frozen", g_hkFrozen);
    SaveHotkeyToJson(hks, L"record", g_hkRecord);
    root.set(L"hotkeys", hks);

    std::wstring serialized = root.serialize(0);
    std::string utf8 = WideToUtf8(serialized);

    std::ofstream fout(WideToUtf8(GetSettingsFilePath()), std::ios::binary);
    if (fout.is_open()) {
        fout.write(utf8.data(), utf8.size());
        fout.close();
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

// Функция поиска верхнего видимого окна под курсором для захвата окон
typedef HRESULT (WINAPI *pfnDwmGetWindowAttribute)(HWND, DWORD, PVOID, DWORD);

struct WindowFindContext {
    POINT pt;
    HWND hOverlayWnd;
    HWND hFoundWnd;
    RECT rcFound;
};

static BOOL CALLBACK EnumWindowsFindProc(HWND hWnd, LPARAM lParam) {
    WindowFindContext* ctx = (WindowFindContext*)lParam;

    // Игнорируем окно оверлея
    if (ctx->hOverlayWnd && hWnd == ctx->hOverlayWnd) return TRUE;

    // Игнорируем рабочий стол и системный shell
    if (hWnd == GetDesktopWindow() || hWnd == GetShellWindow()) return TRUE;

    // Окно должно быть видимым и не свернутым
    if (!IsWindowVisible(hWnd) || IsIconic(hWnd)) return TRUE;

    // Пропускаем прозрачные для мыши окна
    LONG exStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TRANSPARENT) return TRUE;

    // Игнорируем классы рабочего стола Windows (Progman, WorkerW)
    wchar_t clsName[64];
    if (GetClassNameW(hWnd, clsName, 64)) {
        if (wcscmp(clsName, L"Progman") == 0 || wcscmp(clsName, L"WorkerW") == 0) {
            return TRUE;
        }
    }

    // Проверяем cloaked-статус (скрытые или приостановленные UWP приложения в Windows 10/11)
    static pfnDwmGetWindowAttribute s_pDwmGetWindowAttribute = NULL;
    static bool s_dwChecked = false;
    if (!s_dwChecked) {
        HMODULE hDwm = GetModuleHandleW(L"dwmapi.dll");
        if (!hDwm) hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            s_pDwmGetWindowAttribute = (pfnDwmGetWindowAttribute)GetProcAddress(hDwm, "DwmGetWindowAttribute");
        }
        s_dwChecked = true;
    }

    if (s_pDwmGetWindowAttribute) {
        int cloaked = 0;
        if (SUCCEEDED(s_pDwmGetWindowAttribute(hWnd, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof(cloaked))) && cloaked != 0) {
            return TRUE;
        }
    }

    // Получаем реальные видимые границы окна (без невидимой тени DWM)
    RECT rc = { 0, 0, 0, 0 };
    HRESULT hr = E_FAIL;
    if (s_pDwmGetWindowAttribute) {
        hr = s_pDwmGetWindowAttribute(hWnd, 9 /* DWMWA_EXTENDED_FRAME_BOUNDS */, &rc, sizeof(rc));
    }
    if (FAILED(hr)) {
        GetWindowRect(hWnd, &rc);
    }

    // Если окно распахнуто на весь экран (maximized), обрезаем его по границам соответствующего монитора
    if (IsZoomed(hWnd)) {
        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        if (hMon) {
            MONITORINFO mi = { sizeof(MONITORINFO) };
            if (GetMonitorInfoW(hMon, &mi)) {
                IntersectRect(&rc, &rc, &mi.rcMonitor);
            }
        }
    }

    // Игнорируем невидимые или служебные нулевые окна
    if (rc.right - rc.left <= 10 || rc.bottom - rc.top <= 10) return TRUE;

    // Проверяем попадание курсора внутрь окна
    if (PtInRect(&rc, ctx->pt)) {
        ctx->hFoundWnd = hWnd;
        ctx->rcFound = rc;
        return FALSE; // Найдено верхнее видимое окно по Z-order! Останавливаем перебор.
    }

    return TRUE;
}

static void FindTopLevelWindowUnderPoint(POINT pt, HWND hOverlayWnd, HWND* outWnd, RECT* outRect) {
    WindowFindContext ctx;
    ctx.pt = pt;
    ctx.hOverlayWnd = hOverlayWnd;
    ctx.hFoundWnd = NULL;
    ctx.rcFound = { 0, 0, 0, 0 };

    EnumWindows(EnumWindowsFindProc, (LPARAM)&ctx);

    if (outWnd) *outWnd = ctx.hFoundWnd;
    if (outRect) *outRect = ctx.rcFound;
}

// Оконная процедура оверлея
LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        HDC hMemDC = CreateCompatibleDC(hdc);

        if (g_sel.isFrozen && g_sel.hDimmedBmp) {
            // Рисуем затемненный фон
            SelectObject(hMemDC, g_sel.hDimmedBmp);
            BitBlt(hdc, 0, 0, g_sel.screenW, g_sel.screenH, hMemDC, 0, 0, SRCCOPY);

            if (g_sel.isSelecting) {
                // Пользователь тянет рамку вручную: яркий исходный кадр внутри рамки
                int x1 = min(g_sel.startPt.x, g_sel.currentPt.x);
                int y1 = min(g_sel.startPt.y, g_sel.currentPt.y);
                int x2 = max(g_sel.startPt.x, g_sel.currentPt.x);
                int y2 = max(g_sel.startPt.y, g_sel.currentPt.y);
                int rw = x2 - x1;
                int rh = y2 - y1;

                if (rw > 0 && rh > 0 && g_sel.hOriginalBmp) {
                    SelectObject(hMemDC, g_sel.hOriginalBmp);
                    BitBlt(hdc, x1, y1, rw, rh, hMemDC, x1, y1, SRCCOPY);

                    // Чистая неоновая рамка
                    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 229, 255));
                    HGDIOBJ hOldPen = SelectObject(hdc, hPen);
                    HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, x1, y1, x2, y2);
                    SelectObject(hdc, hOldBrush);
                    SelectObject(hdc, hOldPen);
                    DeleteObject(hPen);
                }
            } else if (g_sel.hoveredWnd) {
                // Наведение на окно: окно подсвечивается ярким кадром и неоновой рамкой
                int x1 = g_sel.hoveredRect.left;
                int y1 = g_sel.hoveredRect.top;
                int x2 = g_sel.hoveredRect.right;
                int y2 = g_sel.hoveredRect.bottom;
                int rw = x2 - x1;
                int rh = y2 - y1;

                if (rw > 0 && rh > 0 && g_sel.hOriginalBmp) {
                    SelectObject(hMemDC, g_sel.hOriginalBmp);
                    BitBlt(hdc, x1, y1, rw, rh, hMemDC, x1, y1, SRCCOPY);

                    // Чистая неоновая рамка окна
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
            // В живом режиме (слоистое полупрозрачное окно WS_EX_LAYERED):
            // Очищаем область перерисовки черным цветом (базовый слой прозрачности)
            FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

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
            } else if (g_sel.hoveredWnd) {
                int x1 = g_sel.hoveredRect.left;
                int y1 = g_sel.hoveredRect.top;
                int x2 = g_sel.hoveredRect.right;
                int y2 = g_sel.hoveredRect.bottom;

                // Легкая полупрозрачная подсветка окна
                HBRUSH hFillBrush = CreateSolidBrush(RGB(0, 90, 140));
                RECT rcFill = { x1, y1, x2, y2 };
                FillRect(hdc, &rcFill, hFillBrush);
                DeleteObject(hFillBrush);

                // Неоновая рамка окна
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
        g_sel.isMouseDown = true;
        g_sel.isSelecting = false;
        g_sel.startPt.x = GET_X_LPARAM(lParam);
        g_sel.startPt.y = GET_Y_LPARAM(lParam);
        g_sel.currentPt = g_sel.startPt;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int curX = GET_X_LPARAM(lParam);
        int curY = GET_Y_LPARAM(lParam);

        if (g_sel.isMouseDown) {
            // Кнопка зажата: проверяем порог начала ручного выделения (drag threshold)
            if (!g_sel.isSelecting) {
                int dragX = GetSystemMetrics(SM_CXDRAG);
                int dragY = GetSystemMetrics(SM_CYDRAG);
                if (dragX < 4) dragX = 4;
                if (dragY < 4) dragY = 4;

                if (abs(curX - g_sel.startPt.x) >= dragX || abs(curY - g_sel.startPt.y) >= dragY) {
                    g_sel.isSelecting = true;
                    g_sel.hoveredWnd = NULL; // Ручное выделение отменяет захват окна
                }
            }

            if (g_sel.isSelecting) {
                g_sel.currentPt.x = curX;
                g_sel.currentPt.y = curY;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        } else {
            // Кнопка НЕ зажата: режим наведения на окно
            if (curX != g_sel.lastMouseMovePt.x || curY != g_sel.lastMouseMovePt.y) {
                g_sel.lastMouseMovePt.x = curX;
                g_sel.lastMouseMovePt.y = curY;

                POINT ptScreen = { curX + g_sel.screenX, curY + g_sel.screenY };
                HWND hFound = NULL;
                RECT rcScreen = { 0, 0, 0, 0 };
                FindTopLevelWindowUnderPoint(ptScreen, hWnd, &hFound, &rcScreen);

                RECT rcOverlay = { 0, 0, 0, 0 };
                if (hFound) {
                    int x1 = max(0, (int)rcScreen.left - g_sel.screenX);
                    int y1 = max(0, (int)rcScreen.top - g_sel.screenY);
                    int x2 = min(g_sel.screenW, (int)rcScreen.right - g_sel.screenX);
                    int y2 = min(g_sel.screenH, (int)rcScreen.bottom - g_sel.screenY);
                    if (x2 > x1 && y2 > y1) {
                        rcOverlay.left = x1;
                        rcOverlay.top = y1;
                        rcOverlay.right = x2;
                        rcOverlay.bottom = y2;
                    } else {
                        hFound = NULL;
                    }
                }

                if (hFound != g_sel.hoveredWnd || !EqualRect(&rcOverlay, &g_sel.hoveredRect)) {
                    g_sel.hoveredWnd = hFound;
                    g_sel.hoveredRect = rcOverlay;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!g_sel.isMouseDown) return 0;
        ReleaseCapture();
        g_sel.isMouseDown = false;

        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        bool validCapture = false;

        if (g_sel.isSelecting) {
            // Пользователь самостоятельно выделил прямоугольную область
            g_sel.isSelecting = false;
            x1 = min(g_sel.startPt.x, g_sel.currentPt.x);
            y1 = min(g_sel.startPt.y, g_sel.currentPt.y);
            x2 = max(g_sel.startPt.x, g_sel.currentPt.x);
            y2 = max(g_sel.startPt.y, g_sel.currentPt.y);
            if ((x2 - x1 >= 5) && (y2 - y1 >= 5)) {
                validCapture = true;
            }
        } else if (g_sel.hoveredWnd && (g_sel.hoveredRect.right > g_sel.hoveredRect.left) && (g_sel.hoveredRect.bottom > g_sel.hoveredRect.top)) {
            // Простое нажатие на подсвеченное окно
            x1 = g_sel.hoveredRect.left;
            y1 = g_sel.hoveredRect.top;
            x2 = g_sel.hoveredRect.right;
            y2 = g_sel.hoveredRect.bottom;
            validCapture = true;
        }

        if (!validCapture) {
            DestroyWindow(hWnd);
            ShowNotification(L"Снимок отменён", L"Выделение области прервано");
            return 0;
        }

        int rw = x2 - x1;
        int rh = y2 - y1;

        HBITMAP hCrop = NULL;
        if (g_sel.isFrozen && g_sel.hOriginalBmp) {
            // Вырезаем фрагмент из замороженного в памяти стоп-кадра ДО уничтожения оверлея
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

            DestroyWindow(hWnd);
        } else {
            // Захватываем живую область экрана: сначала скрываем оверлей
            DestroyWindow(hWnd);
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
            g_sel.isMouseDown = false;
            g_sel.isSelecting = false;
            g_sel.hoveredWnd = NULL;
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
        g_sel.hoveredWnd = NULL;
        g_sel.isMouseDown = false;
        g_sel.isSelecting = false;
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Запуск оверлея
void StartOverlay(bool isFrozen) {
    if (g_sel.hOverlayWnd && IsWindow(g_sel.hOverlayWnd)) return;

    g_sel.isFrozen = isFrozen;
    g_sel.isMouseDown = false;
    g_sel.isSelecting = false;
    g_sel.hoveredWnd = NULL;
    g_sel.hoveredRect = { 0, 0, 0, 0 };
    g_sel.lastMouseMovePt = { -1, -1 };
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

    // Определяем окно под курсором сразу при вызове оверлея
    POINT ptCursor;
    if (GetCursorPos(&ptCursor)) {
        HWND hFound = NULL;
        RECT rcScreen = { 0, 0, 0, 0 };
        FindTopLevelWindowUnderPoint(ptCursor, NULL, &hFound, &rcScreen);
        if (hFound) {
            int x1 = max(0, (int)rcScreen.left - g_sel.screenX);
            int y1 = max(0, (int)rcScreen.top - g_sel.screenY);
            int x2 = min(g_sel.screenW, (int)rcScreen.right - g_sel.screenX);
            int y2 = min(g_sel.screenH, (int)rcScreen.bottom - g_sel.screenY);
            if (x2 > x1 && y2 > y1) {
                g_sel.hoveredWnd = hFound;
                g_sel.hoveredRect = { x1, y1, x2, y2 };
            }
        }
        g_sel.lastMouseMovePt = { ptCursor.x - g_sel.screenX, ptCursor.y - g_sel.screenY };
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
            std::wstring targetExe = GetInstalledExePath();
            std::wstring cmd = L"\"" + targetExe + L"\" --autostart";
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

// Завершение любых предыдущих процессов WinScreen (гарантирует единственный процесс)
void KillPreviousInstances() {
    DWORD currentPid = GetCurrentProcessId();

    // 1. Посылаем WM_CLOSE окнам ядра WinScreen_Core других процессов
    HWND hCoreWnd = NULL;
    while ((hCoreWnd = FindWindowW(L"WinScreen_Core", NULL)) != NULL) {
        DWORD pid = 0;
        GetWindowThreadProcessId(hCoreWnd, &pid);
        if (pid == currentPid) break;
        PostMessageW(hCoreWnd, WM_CLOSE, 0, 0);
        SetWindowTextW(hCoreWnd, L"Closing");
        Sleep(40);
    }

    // 2. Ищем все процессы WinScreen.exe и завершаем их
    WCHAR currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);
    const WCHAR* exeName = wcsrchr(currentExePath, L'\\');
    exeName = exeName ? exeName + 1 : currentExePath;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID != currentPid) {
                    bool match = (_wcsicmp(pe.szExeFile, L"WinScreen.exe") == 0) ||
                                 (_wcsicmp(pe.szExeFile, exeName) == 0);
                    if (match) {
                        HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
                        if (hProc) {
                            if (WaitForSingleObject(hProc, 150) == WAIT_TIMEOUT) {
                                TerminateProcess(hProc, 0);
                            }
                            CloseHandle(hProc);
                        }
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
}

bool CopyFileWithRetry(const std::wstring& src, const std::wstring& dst, int maxRetries = 10, int delayMs = 100) {
    for (int i = 0; i < maxRetries; ++i) {
        if (CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
            return true;
        }
        Sleep(delayMs);
    }
    return false;
}

bool PerformInstallOrUpgrade(const std::wstring& currentExe, bool enableAutostart) {
    KillPreviousInstances();

    std::wstring appDir = GetAppDataDir();
    CreateDirectoryW(appDir.c_str(), NULL);
    std::wstring targetExe = GetInstalledExePath();

    if (_wcsicmp(currentExe.c_str(), targetExe.c_str()) != 0) {
        if (!CopyFileWithRetry(currentExe, targetExe)) {
            std::wstring errText = L"Не удалось скопировать исполняемый файл в папку установки:\n" + targetExe +
                L"\n\nПопробуйте закрыть другие программы и повторить попытку.";
            MessageBoxW(NULL, errText.c_str(), L"Ошибка установки", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    DisableWindowsSnippingToolHook();
    SetAutostart(enableAutostart);
    SaveSettings();
    return true;
}

#define WM_INSTALLER_FFMPEG_DONE (WM_USER + 210)

// Окно одноэтапного установщика
static bool s_installConfirmed = false;
static bool s_allowAutostart = true;
static HWND s_hChkAutostart = NULL;
static HWND s_hChkFFmpeg = NULL;
static HWND s_hBtnInstall = NULL;
static HWND s_hBtnCancel = NULL;
static bool s_isDownloadingFFmpeg = false;
static bool s_cancelInstallerDownload = false;

struct InstallerDownloadParams {
    HWND hWnd;
    HWND hBtn;
};

DWORD WINAPI InstallerFFmpegDownloadThread(LPVOID lpParam) {
    InstallerDownloadParams* p = (InstallerDownloadParams*)lpParam;
    HWND hWnd = p->hWnd;
    HWND hBtn = p->hBtn;
    delete p;

    std::wstring destDir = GetAppDataDir();
    CreateDirectoryW(destDir.c_str(), NULL);
    std::wstring dest = destDir + L"\\ffmpeg.exe";
    std::wstring tempDest = dest + L".tmp";

    const WCHAR* url = L"https://github.com/imageio/imageio-binaries/raw/master/ffmpeg/ffmpeg-win64-v4.2.2.exe";

    DownloadProgressCallback cb(hBtn, &s_cancelInstallerDownload);
    HRESULT hr = URLDownloadToFileW(NULL, url, tempDest.c_str(), 0, &cb);

    bool ok = false;
    if (SUCCEEDED(hr) && !s_cancelInstallerDownload) {
        if (MoveFileExW(tempDest.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            ok = true;
        }
    } else {
        DeleteFileW(tempDest.c_str());
    }

    if (IsWindow(hWnd)) {
        PostMessageW(hWnd, WM_INSTALLER_FFMPEG_DONE, ok ? 1 : 0, 0);
    }
    return 0;
}

LRESULT CALLBACK InstallerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        s_isDownloadingFFmpeg = false;
        s_cancelInstallerDownload = false;

        HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hBoldFont = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hTextFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        HICON hIcon = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        if (hIcon) {
            SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }

        HWND lblTitle = CreateWindowW(L"STATIC", (L"Установка WinScreen v" + APP_VERSION).c_str(), WS_CHILD | WS_VISIBLE, 25, 18, 430, 26, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

        HWND lblSubtitle = CreateWindowW(L"STATIC", L"Подтвердить установку", WS_CHILD | WS_VISIBLE, 25, 48, 430, 22, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblSubtitle, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

        HWND lblDesc = CreateWindowW(L"STATIC", L"Быстрая и легковесная утилита для создания скриншотов\nи видеозаписи экрана на Windows 10 / 11.", WS_CHILD | WS_VISIBLE, 25, 75, 430, 36, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(lblDesc, WM_SETFONT, (WPARAM)hTextFont, TRUE);

        s_hChkAutostart = CreateWindowW(L"BUTTON", L"Разрешить автозагрузку (запуск вместе с Windows)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 25, 122, 430, 24, hWnd, (HMENU)301, g_hInstance, NULL);
        SendMessageW(s_hChkAutostart, WM_SETFONT, (WPARAM)hTextFont, TRUE);
        SendMessageW(s_hChkAutostart, BM_SETCHECK, BST_CHECKED, 0);

        bool ffmpegInstalled = IsFFmpegInstalled();
        std::wstring ffmpegText = ffmpegInstalled 
            ? L"Установить FFmpeg (уже установлено)" 
            : L"Установить FFmpeg (для видеозаписи со звуком)";

        s_hChkFFmpeg = CreateWindowW(L"BUTTON", ffmpegText.c_str(), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 25, 156, 430, 24, hWnd, (HMENU)302, g_hInstance, NULL);
        SendMessageW(s_hChkFFmpeg, WM_SETFONT, (WPARAM)hTextFont, TRUE);
        SendMessageW(s_hChkFFmpeg, BM_SETCHECK, BST_CHECKED, 0);

        if (ffmpegInstalled) {
            EnableWindow(s_hChkFFmpeg, FALSE);
        }

        s_hBtnInstall = CreateWindowW(L"BUTTON", L"Подтвердить установку", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 125, 215, 215, 38, hWnd, (HMENU)303, g_hInstance, NULL);
        SendMessageW(s_hBtnInstall, WM_SETFONT, (WPARAM)hBoldFont, TRUE);

        s_hBtnCancel = CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE, 350, 219, 95, 30, hWnd, (HMENU)304, g_hInstance, NULL);
        SendMessageW(s_hBtnCancel, WM_SETFONT, (WPARAM)hTextFont, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 302) { // FFmpeg checkbox
            if (IsFFmpegInstalled()) {
                SendMessageW(s_hChkFFmpeg, BM_SETCHECK, BST_CHECKED, 0);
            }
        } else if (id == 303) { // Подтвердить установку
            if (s_isDownloadingFFmpeg) return 0;

            bool needFFmpeg = (!IsFFmpegInstalled() && (SendMessageW(s_hChkFFmpeg, BM_GETCHECK, 0, 0) == BST_CHECKED));

            if (needFFmpeg) {
                s_isDownloadingFFmpeg = true;
                EnableWindow(s_hBtnInstall, FALSE);
                EnableWindow(s_hBtnCancel, FALSE);
                EnableWindow(s_hChkAutostart, FALSE);
                EnableWindow(s_hChkFFmpeg, FALSE);
                SetWindowTextW(s_hBtnInstall, L"⏳ Загрузка FFmpeg (0%)...");

                InstallerDownloadParams* param = new InstallerDownloadParams{ hWnd, s_hBtnInstall };
                CreateThread(NULL, 0, InstallerFFmpegDownloadThread, param, 0, NULL);
            } else {
                s_allowAutostart = (SendMessageW(s_hChkAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                s_installConfirmed = true;
                DestroyWindow(hWnd);
            }
        } else if (id == 304) { // Отмена
            if (s_isDownloadingFFmpeg) {
                s_cancelInstallerDownload = true;
            }
            s_installConfirmed = false;
            DestroyWindow(hWnd);
        }
        return 0;
    }
    case WM_INSTALLER_FFMPEG_DONE: {
        s_isDownloadingFFmpeg = false;
        bool ok = (wParam == 1);
        if (ok) {
            s_allowAutostart = (SendMessageW(s_hChkAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_installConfirmed = true;
            DestroyWindow(hWnd);
        } else {
            int res = MessageBoxW(hWnd,
                L"Не удалось скачать FFmpeg (проверьте подключение к Интернету).\n\n"
                L"Продолжить установку WinScreen без FFmpeg?\n"
                L"(Вы сможете установить FFmpeg позже в настройках программы)",
                L"Загрузка FFmpeg", MB_YESNO | MB_ICONWARNING);
            if (res == IDYES) {
                s_allowAutostart = (SendMessageW(s_hChkAutostart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                s_installConfirmed = true;
                DestroyWindow(hWnd);
            } else {
                EnableWindow(s_hBtnInstall, TRUE);
                EnableWindow(s_hBtnCancel, TRUE);
                EnableWindow(s_hChkAutostart, TRUE);
                EnableWindow(s_hChkFFmpeg, TRUE);
                SetWindowTextW(s_hBtnInstall, L"Подтвердить установку");
            }
        }
        return 0;
    }
    case WM_CLOSE: {
        if (s_isDownloadingFFmpeg) {
            s_cancelInstallerDownload = true;
        }
        s_installConfirmed = false;
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool ShowInstallerDialog(HINSTANCE hInstance, bool& outAutostart) {
    s_installConfirmed = false;
    s_allowAutostart = true;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = InstallerWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WinScreen_Installer";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    RegisterClassExW(&wc);

    int w = 480;
    int h = 315;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hWnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"WinScreen — Установка",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        sx, sy, w, h,
        NULL, NULL, hInstance, NULL
    );

    SetForegroundWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(wc.lpszClassName, hInstance);
    outAutostart = s_allowAutostart;
    return s_installConfirmed;
}

void UninstallApp() {
    SetAutostart(false);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEYBOARD_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 1;
        RegSetValueExW(hKey, REG_SNIP_KEY, 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    if (g_nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
    }

    std::wstring installedExe = GetInstalledExePath();

    MessageBoxW(NULL,
        L"WinScreen успешно удалён из автозагрузки и системы.\n\n"
        L"Файл настроек (settings.json) сохранён,\n"
        L"чтобы ваши горячие клавиши и параметры не потерялись при будущей установке.",
        L"WinScreen", MB_OK | MB_ICONINFORMATION);

    // Гарантированное удаление исполняемого файла через bat-скрипт после выхода из процесса
    std::wstring tempBat = GetAppDataDir() + L"\\_ws_uninstall.bat";
    std::wofstream bat(tempBat);
    if (bat.is_open()) {
        bat << L"@echo off\n";
        bat << L"chcp 65001 >nul\n";
        bat << L"timeout /t 1 /nobreak >nul\n";
        bat << L":retry\n";
        bat << L"taskkill /f /im WinScreen.exe >nul 2>&1\n";
        bat << L"del /f /q \"" << installedExe << L"\" >nul 2>&1\n";
        bat << L"if exist \"" << installedExe << L"\" (\n";
        bat << L"    timeout /t 1 /nobreak >nul\n";
        bat << L"    goto retry\n";
        bat << L")\n";
        bat << L"del /f /q \"%~f0\" >nul 2>&1\n";
        bat.close();
        ShellExecuteW(NULL, L"open", tempBat.c_str(), NULL, NULL, SW_HIDE);
    }

    ExitProcess(0);
}

void OpenScreenshotsFolder() {
    CreateDirectoryW(g_saveDir.c_str(), NULL);
    ShellExecuteW(NULL, L"open", g_saveDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

// Генерация значка трея (из встроенных ресурсов или в памяти)
HICON CreateTrayIcon() {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HICON hRes = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
    if (hRes) return hRes;

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

// Структура и логика проверки обновлений
struct UpdateInfo {
    bool hasUpdate = false;
    std::wstring latestVersion = L"";
    std::wstring releaseUrl = L"";
    std::wstring downloadUrl = L"";
};

static UpdateInfo g_updateInfo;
static std::mutex g_updateMutex;

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

static Version ParseVersion(const std::wstring& str) {
    Version v;
    std::wstring s = str;
    while (!s.empty() && (s[0] == L'v' || s[0] == L'V' || s[0] == L' ')) {
        s = s.substr(1);
    }
    int parts[3] = { 0, 0, 0 };
    int idx = 0;
    size_t start = 0;
    for (size_t i = 0; i <= s.length() && idx < 3; ++i) {
        if (i == s.length() || s[i] == L'.') {
            if (i > start) {
                try {
                    parts[idx] = std::stoi(s.substr(start, i - start));
                } catch (...) {
                    parts[idx] = 0;
                }
            }
            idx++;
            start = i + 1;
        }
    }
    v.major = parts[0];
    v.minor = parts[1];
    v.patch = parts[2];
    return v;
}

static bool IsNewerVersion(const std::wstring& remoteStr, const std::wstring& currentStr) {
    Version r = ParseVersion(remoteStr);
    Version c = ParseVersion(currentStr);
    if (r.major != c.major) return r.major > c.major;
    if (r.minor != c.minor) return r.minor > c.minor;
    return r.patch > c.patch;
}

static std::wstring ExtractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return L"";
    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }
    if (pos < json.length() && json[pos] == '\"') {
        pos++;
        size_t endPos = json.find('\"', pos);
        if (endPos != std::string::npos) {
            std::string val = json.substr(pos, endPos - pos);
            return Utf8ToWide(val);
        }
    }
    return L"";
}

static std::wstring ExtractExeDownloadUrl(const std::string& json) {
    std::string needle = "\"browser_download_url\"";
    size_t pos = 0;
    while ((pos = json.find(needle, pos)) != std::string::npos) {
        pos += needle.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
            pos++;
        }
        if (pos < json.length() && json[pos] == '\"') {
            pos++;
            size_t endPos = json.find('\"', pos);
            if (endPos != std::string::npos) {
                std::string url = json.substr(pos, endPos - pos);
                if (url.find(".exe") != std::string::npos) {
                    return Utf8ToWide(url);
                }
                pos = endPos + 1;
            }
        }
    }
    return L"";
}

static bool CheckForUpdates(UpdateInfo& outInfo) {
    HINTERNET hInternet = InternetOpenW(L"WinScreen-UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
    HINTERNET hUrl = InternetOpenUrlW(hInternet, GITHUB_API_LATEST_RELEASE.c_str(), NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    std::string json;
    char buf[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        json.append(buf, bytesRead);
    }
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (json.empty()) return false;

    std::wstring tagName = ExtractJsonString(json, "tag_name");
    std::wstring htmlUrl = ExtractJsonString(json, "html_url");
    std::wstring downloadUrl = ExtractExeDownloadUrl(json);

    if (tagName.empty()) return false;

    outInfo.latestVersion = tagName;
    outInfo.releaseUrl = htmlUrl.empty() ? (GITHUB_REPO_URL + L"/releases/latest") : htmlUrl;
    outInfo.downloadUrl = downloadUrl.empty() ? (GITHUB_REPO_URL + L"/releases/download/" + tagName + L"/WinScreen.exe") : downloadUrl;
    outInfo.hasUpdate = IsNewerVersion(tagName, APP_VERSION);

    return true;
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
    static HWND s_hLblVersion = NULL;
    static HWND s_hBtnGitHub = NULL;
    static HWND s_hBtnUpdate = NULL;
    static std::vector<std::wstring> s_detectedMics;

    switch (msg) {
    case WM_UPDATE_CHECK_DONE: {
        std::lock_guard<std::mutex> lock(g_updateMutex);
        if (g_updateInfo.hasUpdate && s_hBtnUpdate && IsWindow(s_hBtnUpdate)) {
            std::wstring btnText = L"🚀 Обновить (" + g_updateInfo.latestVersion + L")";
            SetWindowTextW(s_hBtnUpdate, btnText.c_str());
            ShowWindow(s_hBtnUpdate, SW_SHOW);
        }
        return 0;
    }
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

        // 8. Подвал: версия, кнопка обновления и переход на GitHub
        s_hLblVersion = CreateWindowW(L"STATIC", (L"WinScreen v" + APP_VERSION).c_str(), WS_CHILD | WS_VISIBLE, 20, 508, 140, 20, hWnd, NULL, g_hInstance, NULL);
        SendMessageW(s_hLblVersion, WM_SETFONT, (WPARAM)hFont, TRUE);

        s_hBtnUpdate = CreateWindowW(L"BUTTON", L"🚀 Обновить", WS_CHILD, 165, 503, 180, 26, hWnd, (HMENU)113, g_hInstance, NULL);
        SendMessageW(s_hBtnUpdate, WM_SETFONT, (WPARAM)hFont, TRUE);

        s_hBtnGitHub = CreateWindowW(L"BUTTON", L"GitHub", WS_CHILD | WS_VISIBLE, 415, 503, 85, 26, hWnd, (HMENU)112, g_hInstance, NULL);
        SendMessageW(s_hBtnGitHub, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Проверяем, было ли уже обнаружено обновление
        {
            std::lock_guard<std::mutex> lock(g_updateMutex);
            if (g_updateInfo.hasUpdate) {
                std::wstring btnText = L"🚀 Обновить (" + g_updateInfo.latestVersion + L")";
                SetWindowTextW(s_hBtnUpdate, btnText.c_str());
                ShowWindow(s_hBtnUpdate, SW_SHOW);
            }
        }

        // Запуск проверки обновлений в фоновом потоке
        std::thread([hWnd]() {
            UpdateInfo info;
            if (CheckForUpdates(info)) {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                g_updateInfo = info;
                if (IsWindow(hWnd)) {
                    PostMessageW(hWnd, WM_UPDATE_CHECK_DONE, 0, 0);
                }
            }
        }).detach();

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
            if (MessageBoxW(hWnd, L"Вы уверены, что хотите полностью удалить WinScreen из системы?", L"Удаление WinScreen", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                UninstallApp();
            }
        } else if (id == 112) { // Кнопка GitHub
            ShellExecuteW(NULL, L"open", GITHUB_REPO_URL.c_str(), NULL, NULL, SW_SHOWNORMAL);
        } else if (id == 113) { // Кнопка Обновить
            std::wstring latestVer, relUrl, dlUrl;
            {
                std::lock_guard<std::mutex> lock(g_updateMutex);
                latestVer = g_updateInfo.latestVersion;
                relUrl = g_updateInfo.releaseUrl;
                dlUrl = g_updateInfo.downloadUrl;
            }

            std::wstring msg = L"Доступна новая версия " + (latestVer.empty() ? L"" : latestVer) + L"!\n\n"
                               L"Нажмите «Да», чтобы обновить приложение автоматически без переустановки.\n"
                               L"Нажмите «Нет», чтобы перейти на страницу релиза на GitHub.\n"
                               L"Нажмите «Отмена», чтобы закрыть окно.";

            int choice = MessageBoxW(hWnd, msg.c_str(), L"Обновление WinScreen", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (choice == IDNO) {
                ShellExecuteW(NULL, L"open", relUrl.empty() ? GITHUB_REPO_URL.c_str() : relUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else if (choice == IDYES) {
                EnableWindow(s_hBtnUpdate, FALSE);
                SetWindowTextW(s_hBtnUpdate, L"⏳ Загрузка обновления...");

                std::thread([hWnd, dlUrl, relUrl]() {
                    WCHAR currentExe[MAX_PATH];
                    GetModuleFileNameW(NULL, currentExe, MAX_PATH);

                    WCHAR tempDir[MAX_PATH];
                    GetTempPathW(MAX_PATH, tempDir);
                    std::wstring tempNewExe = std::wstring(tempDir) + L"WinScreen_update.exe";
                    std::wstring tempBat = std::wstring(tempDir) + L"winscreen_updater.bat";

                    DeleteFileW(tempNewExe.c_str());
                    HRESULT hr = URLDownloadToFileW(NULL, dlUrl.c_str(), tempNewExe.c_str(), 0, NULL);
                    if (FAILED(hr)) {
                        if (IsWindow(hWnd)) {
                            EnableWindow(s_hBtnUpdate, TRUE);
                            std::wstring btnText = L"🚀 Обновить";
                            {
                                std::lock_guard<std::mutex> lock(g_updateMutex);
                                if (!g_updateInfo.latestVersion.empty()) {
                                    btnText += L" (" + g_updateInfo.latestVersion + L")";
                                }
                            }
                            SetWindowTextW(s_hBtnUpdate, btnText.c_str());
                        }

                        int openWeb = MessageBoxW(hWnd,
                            L"Не удалось автоматически загрузить файл обновления.\nОткрыть страницу релиза на GitHub?",
                            L"Ошибка обновления", MB_YESNO | MB_ICONERROR);
                        if (openWeb == IDYES) {
                            ShellExecuteW(NULL, L"open", relUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                        return;
                    }

                    // Создаем bat-скрипт для авто-обновления без переустановки
                    std::wofstream bat(tempBat);
                    if (bat.is_open()) {
                        bat << L"@echo off\n";
                        bat << L"chcp 65001 >nul\n";
                        bat << L"timeout /t 1 /nobreak >nul\n";
                        bat << L":retry\n";
                        bat << L"del /f /q \"" << currentExe << L"\" >nul 2>&1\n";
                        bat << L"if exist \"" << currentExe << L"\" (\n";
                        bat << L"    timeout /t 1 /nobreak >nul\n";
                        bat << L"    goto retry\n";
                        bat << L")\n";
                        bat << L"move /y \"" << tempNewExe << L"\" \"" << currentExe << L"\" >nul 2>&1\n";
                        bat << L"start \"\" \"" << currentExe << L"\"\n";
                        bat << L"del /f /q \"%~f0\" >nul 2>&1\n";
                        bat.close();

                        ShellExecuteW(NULL, L"open", tempBat.c_str(), NULL, NULL, SW_HIDE);
                        ExitProcess(0);
                    }
                }).detach();
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
        s_hLblVersion = s_hBtnGitHub = s_hBtnUpdate = NULL;
        s_detectedMics.clear();
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void OpenSettingsDialog() {
    if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) {
        ShowWindow(g_hSettingsWnd, SW_RESTORE);
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
    if (!GetClassInfoExW(g_hInstance, L"WinScreen_Settings", &wc)) {
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = L"WinScreen_Settings";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(1));
        RegisterClassExW(&wc);
    }

    int w = 535, h = 580;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hSettingsWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"WinScreen_Settings",
        L"WinScreen — Настройки",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        sx, sy, w, h,
        NULL, NULL, g_hInstance, NULL
    );
    if (g_hSettingsWnd) {
        HICON hIcon = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        if (hIcon) {
            SendMessageW(g_hSettingsWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(g_hSettingsWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
        SetForegroundWindow(g_hSettingsWnd);
    }
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
    } else if (cmd == L"--help" || cmd == L"-h") {
        MessageBoxW(NULL,
            L"WinScreen — Параметры командной строки:\n\n"
            L"  --settings       Открыть графическое окно настроек\n"
            L"  --status         Показать текущую конфигурацию и горячие клавиши\n"
            L"  --uninstall      Удалить из автозагрузки и системы\n"
            L"  --autostart      Фоновый запуск при старте Windows (сворачивание в трей)\n\n"
            L"Запуск без параметров активирует службу скриншотов и сворачивает программу в системный трей.",
            L"WinScreen — Справка", MB_OK | MB_ICONINFORMATION);
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    } else if (cmd == L"--status") {
        std::wstring status = L"Текущая конфигурация WinScreen (v" + APP_VERSION + L"):\n\n"
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

    WCHAR currentExePath[MAX_PATH];
    GetModuleFileNameW(NULL, currentExePath, MAX_PATH);

    std::wstring installedExe = GetInstalledExePath();
    bool isRunningInstalled = (_wcsicmp(currentExePath, installedExe.c_str()) == 0);
    bool isInstalled = IsWinScreenInstalled();
    std::wstring installedVer = GetInstalledVersion();

    bool isAutostart = (cmd == L"--autostart" || cmd == L"--silent" || cmd == L"-s");
    bool isExplicitSettings = (cmd == L"--settings");

    if (!isRunningInstalled) {
        // Запуск не из установленного расположения (%APPDATA%\WinScreen\WinScreen.exe)
        if (!isInstalled) {
            // Пункт 2: WinScreen еще не установлен в системе -> Показываем одноэтапный установщик
            bool allowAutostart = true;
            if (!ShowInstallerDialog(hInstance, allowAutostart)) {
                // Пользователь отменил установку
                GdiplusShutdown(g_gdiplusToken);
                return 0;
            }

            // Установка подтверждена пользователем
            if (!PerformInstallOrUpgrade(currentExePath, allowAutostart)) {
                GdiplusShutdown(g_gdiplusToken);
                return 1;
            }

            // После установки открываем настройки установленного WinScreen
            ShellExecuteW(NULL, L"open", installedExe.c_str(), L"--settings", NULL, SW_SHOWNORMAL);
            GdiplusShutdown(g_gdiplusToken);
            return 0;
        } else {
            // WinScreen уже установлен в системе
            int cmp = CompareVersions(APP_VERSION, installedVer);
            if (cmp < 0) {
                // Пункт 3: Попытка открыть более старую версию
                std::wstring msg = L"WinScreen уже установлен в системе (версия v" + installedVer + L").\n\n"
                    L"Вы пытаетесь открыть более старую версию (v" + APP_VERSION + L").\n"
                    L"Установить старую версию без удаления текущей невозможно.\n\n"
                    L"Для удаления текущей версии откройте Настройки WinScreen -> «Удалить из системы».";
                MessageBoxW(NULL, msg.c_str(), L"WinScreen — Предупреждение", MB_OK | MB_ICONWARNING);
                GdiplusShutdown(g_gdiplusToken);
                return 0;
            } else if (cmp == 0) {
                // Пункт 4: Уже установлена та же версия -> просто открываются настройки
                ShellExecuteW(NULL, L"open", installedExe.c_str(), L"--settings", NULL, SW_SHOWNORMAL);
                GdiplusShutdown(g_gdiplusToken);
                return 0;
            } else {
                // Пункт 5: Попытка открыть более новую версию -> предлагаем обновиться
                std::wstring msg = L"Ваша текущая версия WinScreen v" + installedVer + L".\n"
                    L"Вы пытаетесь открыть новую версию WinScreen v" + APP_VERSION + L".\n\n"
                    L"Хотите обновиться?";
                int res = MessageBoxW(NULL, msg.c_str(), L"WinScreen — Обновление", MB_YESNO | MB_ICONQUESTION);
                if (res == IDYES) {
                    bool keepAutostart = IsAutostartEnabled();
                    if (!PerformInstallOrUpgrade(currentExePath, keepAutostart)) {
                        GdiplusShutdown(g_gdiplusToken);
                        return 1;
                    }
                    ShellExecuteW(NULL, L"open", installedExe.c_str(), L"--settings", NULL, SW_SHOWNORMAL);
                    GdiplusShutdown(g_gdiplusToken);
                    return 0;
                } else {
                    GdiplusShutdown(g_gdiplusToken);
                    return 0;
                }
            }
        }
    }

    // Если процесс выполняется из целевого каталога установки:
    // Пункт 7: Гарантируем, что у WinScreen всегда ровно один процесс!
    // При запуске нового процесса прошлые автоматически закрываются.
    KillPreviousInstances();

    // Создание скрытого служебного окна
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"WinScreen_Core";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
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

    // Если запуск не фоновый из автозагрузки (или запрошен --settings) -> открываем настройки
    if (!isAutostart || isExplicitSettings) {
        OpenSettingsDialog();
    }

    // Фоновая проверка наличия обновлений при запуске
    std::thread([]() {
        Sleep(2500);
        UpdateInfo info;
        if (CheckForUpdates(info)) {
            std::lock_guard<std::mutex> lock(g_updateMutex);
            g_updateInfo = info;
            if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) {
                PostMessageW(g_hSettingsWnd, WM_UPDATE_CHECK_DONE, 0, 0);
            }
        }
    }).detach();

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
