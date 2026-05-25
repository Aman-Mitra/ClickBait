#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;

namespace {

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef NIM_SETVERSION
#define NIM_SETVERSION 0x00000004
#endif

#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4 4
#endif

#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif

#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif

constexpr wchar_t kAppClassName[] = L"ClickBaitMainWindow";
constexpr wchar_t kPulseClassName[] = L"ClickBaitPulseWindow";
constexpr wchar_t kSettingsClassName[] = L"ClickBaitSettingsWindow";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT_PTR kPulseTimerId = 1;
constexpr UINT kPulseFrameMs = 24;
constexpr wchar_t kRepoUrl[] = L"https://github.com/Aman-Mitra/ClickBait";
constexpr int kAppIconResourceId = 101;

enum class ClickKind {
    LeftDown,
    LeftUp,
    RightDown,
    RightUp,
    Drag
};

enum class ColorPreset {
    Default,
    Blue,
    Green,
    Purple,
    Pink,
    Orange,
    White
};

struct Settings {
    bool enabled = true;
    bool showPress = true;
    bool showRelease = true;
    bool showRightClick = true;
    bool showDrag = true;
    bool compactTrayIcon = false;
    double size = 64.0;
    double intensity = 0.9;
    double duration = 0.48;
    ColorPreset colorPreset = ColorPreset::Default;
};

struct RecentEvent {
    ClickKind kind;
    POINT point;
    ULONGLONG tick;
};

struct PulseWindow {
    HWND hwnd = nullptr;
    ClickKind kind = ClickKind::LeftDown;
    POINT screenPoint{};
    ULONGLONG startTick = 0;
    double durationMs = 480.0;
    double baseSize = 64.0;
    double intensity = 0.9;
    Color color = Color(255, 0, 188, 255);
    int bitmapSize = 220;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND mainWindow = nullptr;
    HWND settingsWindow = nullptr;
    HHOOK mouseHook = nullptr;
    NOTIFYICONDATAW trayIcon{};
    HICON activeRegularIcon = nullptr;
    HICON activeCompactIcon = nullptr;
    HICON inactiveRegularIcon = nullptr;
    HICON inactiveCompactIcon = nullptr;
    Settings settings;
    std::wstring settingsPath;
    ULONG_PTR gdiplusToken = 0;
    bool leftButtonDown = false;
    bool rightButtonDown = false;
    POINT lastDragPoint{};
    ULONGLONG lastDragTick = 0;
    std::vector<RecentEvent> recentEvents;
    std::vector<PulseWindow*> pulses;
};

AppState gApp;

void RefreshHookState();
void RefreshTrayIcon();
void ShowPulse(ClickKind kind, POINT point);
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

enum MenuId : UINT {
    ID_TOGGLE_ENABLED = 100,
    ID_TOGGLE_PRESS = 101,
    ID_TOGGLE_RELEASE = 102,
    ID_TOGGLE_RIGHT_CLICK = 103,
    ID_TOGGLE_DRAG = 104,
    ID_TOGGLE_COMPACT_ICON = 105,
    ID_SIZE_SMALL = 200,
    ID_SIZE_MEDIUM = 201,
    ID_SIZE_LARGE = 202,
    ID_SIZE_HUGE = 203,
    ID_INTENSITY_SUBTLE = 300,
    ID_INTENSITY_NORMAL = 301,
    ID_INTENSITY_BRIGHT = 302,
    ID_INTENSITY_BEACON = 303,
    ID_DURATION_SNAPPY = 400,
    ID_DURATION_NORMAL = 401,
    ID_DURATION_SLOW = 402,
    ID_DURATION_VERY_SLOW = 403,
    ID_COLOR_DEFAULT = 500,
    ID_COLOR_BLUE = 501,
    ID_COLOR_GREEN = 502,
    ID_COLOR_PURPLE = 503,
    ID_COLOR_PINK = 504,
    ID_COLOR_ORANGE = 505,
    ID_COLOR_WHITE = 506,
    ID_TEST_PULSE = 600,
    ID_OPEN_SETTINGS = 601,
    ID_CHECK_UPDATES = 602,
    ID_QUIT = 603
};

enum ControlId : int {
    CID_ENABLED = 1000,
    CID_SHOW_PRESS = 1001,
    CID_SHOW_RELEASE = 1002,
    CID_SHOW_RIGHT = 1003,
    CID_SHOW_DRAG = 1004,
    CID_COMPACT_ICON = 1005,
    CID_SIZE = 1010,
    CID_INTENSITY = 1011,
    CID_DURATION = 1012,
    CID_COLOR = 1013,
    CID_TEST = 1020,
    CID_CLOSE = 1021
};

double Clamp(double value, double minValue, double maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

template <typename T, size_t N>
size_t ArrayCount(const T (&)[N]) {
    return N;
}

struct OptionItem {
    const wchar_t* label;
    double value;
};

const OptionItem kSizeOptions[] = {
    {L"Small", 44.0},
    {L"Medium", 64.0},
    {L"Large", 88.0},
    {L"Huge", 116.0},
};

const OptionItem kIntensityOptions[] = {
    {L"Subtle", 0.28},
    {L"Normal", 0.70},
    {L"Bright", 1.00},
    {L"Beacon", 1.35},
};

const OptionItem kDurationOptions[] = {
    {L"Snappy", 0.28},
    {L"Normal", 0.48},
    {L"Slow", 0.72},
    {L"Very Slow", 1.00},
};

struct ColorOptionItem {
    const wchar_t* label;
    ColorPreset preset;
};

const ColorOptionItem kColorOptions[] = {
    {L"Default", ColorPreset::Default},
    {L"Blue", ColorPreset::Blue},
    {L"Green", ColorPreset::Green},
    {L"Purple", ColorPreset::Purple},
    {L"Pink", ColorPreset::Pink},
    {L"Orange", ColorPreset::Orange},
    {L"White", ColorPreset::White},
};

Color MakeColor(BYTE alpha, BYTE r, BYTE g, BYTE b) {
    return Color(alpha, r, g, b);
}

Color PresetColor(ColorPreset preset) {
    switch (preset) {
    case ColorPreset::Blue:
        return Color(255, 0, 188, 255);
    case ColorPreset::Green:
        return Color(255, 51, 230, 107);
    case ColorPreset::Purple:
        return Color(255, 148, 92, 255);
    case ColorPreset::Pink:
        return Color(255, 255, 82, 184);
    case ColorPreset::Orange:
        return Color(255, 255, 117, 48);
    case ColorPreset::White:
        return Color(255, 255, 255, 255);
    case ColorPreset::Default:
    default:
        return Color(255, 0, 0, 0);
    }
}

Color EventColor(const Settings& settings, ClickKind kind) {
    if (settings.colorPreset != ColorPreset::Default) {
        return PresetColor(settings.colorPreset);
    }

    switch (kind) {
    case ClickKind::LeftDown:
        return Color(255, 0, 188, 255);
    case ClickKind::LeftUp:
        return Color(255, 102, 224, 255);
    case ClickKind::RightDown:
    case ClickKind::RightUp:
        return Color(255, 255, 117, 48);
    case ClickKind::Drag:
        return Color(255, 234, 214, 56);
    }

    return Color(255, 255, 255, 255);
}

double EventDurationMs(const Settings& settings, ClickKind kind) {
    switch (kind) {
    case ClickKind::Drag:
        return std::min(380.0, settings.duration * 1000.0 * 0.82);
    case ClickKind::LeftUp:
    case ClickKind::RightUp:
        return settings.duration * 1000.0 * 0.78;
    case ClickKind::LeftDown:
    case ClickKind::RightDown:
    default:
        return settings.duration * 1000.0;
    }
}

double EventBaseSize(const Settings& settings, ClickKind kind) {
    switch (kind) {
    case ClickKind::Drag:
        return settings.size * 0.6;
    case ClickKind::LeftUp:
    case ClickKind::RightUp:
        return settings.size * 0.82;
    case ClickKind::LeftDown:
    case ClickKind::RightDown:
    default:
        return settings.size;
    }
}

std::wstring SettingsDirectory() {
    wchar_t appData[MAX_PATH] = {};
    DWORD result = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (result == 0 || result >= MAX_PATH) {
        return L".";
    }

    std::wstring directory = appData;
    directory += L"\\ClickBait";
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory;
}

std::wstring BuildSettingsPath() {
    std::wstring path = SettingsDirectory();
    path += L"\\settings.ini";
    return path;
}

void SaveSettings() {
    const wchar_t* path = gApp.settingsPath.c_str();
    WritePrivateProfileStringW(L"ClickBait", L"enabled", gApp.settings.enabled ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"ClickBait", L"showPress", gApp.settings.showPress ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"ClickBait", L"showRelease", gApp.settings.showRelease ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"ClickBait", L"showRightClick", gApp.settings.showRightClick ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"ClickBait", L"showDrag", gApp.settings.showDrag ? L"1" : L"0", path);
    WritePrivateProfileStringW(L"ClickBait", L"compactTrayIcon", gApp.settings.compactTrayIcon ? L"1" : L"0", path);

    wchar_t buffer[64];
    swprintf(buffer, L"%.2f", gApp.settings.size);
    WritePrivateProfileStringW(L"ClickBait", L"size", buffer, path);
    swprintf(buffer, L"%.2f", gApp.settings.intensity);
    WritePrivateProfileStringW(L"ClickBait", L"intensity", buffer, path);
    swprintf(buffer, L"%.2f", gApp.settings.duration);
    WritePrivateProfileStringW(L"ClickBait", L"duration", buffer, path);
    swprintf(buffer, L"%d", static_cast<int>(gApp.settings.colorPreset));
    WritePrivateProfileStringW(L"ClickBait", L"colorPreset", buffer, path);
}

double ReadDouble(const wchar_t* key, double defaultValue) {
    wchar_t buffer[64] = {};
    wchar_t fallback[64] = {};
    swprintf(fallback, L"%.2f", defaultValue);
    GetPrivateProfileStringW(L"ClickBait", key, fallback, buffer, 64, gApp.settingsPath.c_str());
    return _wtof(buffer);
}

int ReadInt(const wchar_t* key, int defaultValue) {
    return GetPrivateProfileIntW(L"ClickBait", key, defaultValue, gApp.settingsPath.c_str());
}

void LoadSettings() {
    gApp.settingsPath = BuildSettingsPath();
    gApp.settings.enabled = ReadInt(L"enabled", 1) != 0;
    gApp.settings.showPress = ReadInt(L"showPress", 1) != 0;
    gApp.settings.showRelease = ReadInt(L"showRelease", 1) != 0;
    gApp.settings.showRightClick = ReadInt(L"showRightClick", 1) != 0;
    gApp.settings.showDrag = ReadInt(L"showDrag", 1) != 0;
    gApp.settings.compactTrayIcon = ReadInt(L"compactTrayIcon", 0) != 0;
    gApp.settings.size = Clamp(ReadDouble(L"size", 64.0), 44.0, 116.0);
    gApp.settings.intensity = Clamp(ReadDouble(L"intensity", 0.9), 0.28, 1.35);
    gApp.settings.duration = Clamp(ReadDouble(L"duration", 0.48), 0.28, 1.0);
    int colorValue = ReadInt(L"colorPreset", 0);
    if (colorValue < 0 || colorValue > static_cast<int>(ColorPreset::White)) {
        colorValue = 0;
    }
    gApp.settings.colorPreset = static_cast<ColorPreset>(colorValue);
}

HICON BuildTrayIcon(bool compact, bool enabled) {
    const int size = GetSystemMetrics(SM_CXSMICON);
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HBITMAP colorBitmap = CreateDIBSection(screenDc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, colorBitmap);

    Graphics graphics(memoryDc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));

    const Color accent = enabled ? Color(255, 0, 188, 255) : Color(255, 145, 145, 145);
    const Color fill = enabled ? Color(220, 0, 188, 255) : Color(190, 145, 145, 145);
    Pen pen(accent, compact ? 2.2f : 1.8f);
    SolidBrush brush(fill);
    RectF bounds(2.0f, 2.0f, static_cast<REAL>(size - 4), static_cast<REAL>(size - 4));

    if (compact) {
        graphics.DrawEllipse(&pen, bounds);
    } else {
        graphics.DrawEllipse(&pen, bounds.X + 1.5f, bounds.Y + 1.5f, bounds.Width - 3.0f, bounds.Height - 3.0f);
        graphics.FillEllipse(&brush, RectF(size * 0.38f, size * 0.38f, size * 0.24f, size * 0.24f));
    }

    if (!enabled) {
        Pen slashPen(Color(210, 235, 235, 235), 1.8f);
        graphics.DrawLine(&slashPen, size * 0.25f, size * 0.75f, size * 0.75f, size * 0.25f);
    }

    HBITMAP maskBitmap = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmColor = colorBitmap;
    iconInfo.hbmMask = maskBitmap;
    HICON icon = CreateIconIndirect(&iconInfo);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(maskBitmap);
    DeleteObject(colorBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    return icon;
}

void RefreshTrayIcon() {
    if (gApp.settings.enabled) {
        gApp.trayIcon.hIcon = gApp.settings.compactTrayIcon ? gApp.activeCompactIcon : gApp.activeRegularIcon;
        lstrcpynW(gApp.trayIcon.szTip, L"ClickBait (On)", ARRAYSIZE(gApp.trayIcon.szTip));
    } else {
        gApp.trayIcon.hIcon = gApp.settings.compactTrayIcon ? gApp.inactiveCompactIcon : gApp.inactiveRegularIcon;
        lstrcpynW(gApp.trayIcon.szTip, L"ClickBait (Off)", ARRAYSIZE(gApp.trayIcon.szTip));
    }
    Shell_NotifyIconW(NIM_MODIFY, &gApp.trayIcon);
}

void SyncSettingsWindow();

void ApplySettingsChanged(bool refreshHook = false, bool refreshTray = false) {
    SaveSettings();
    if (refreshHook) {
        RefreshHookState();
    }
    if (refreshTray) {
        RefreshTrayIcon();
    }
    SyncSettingsWindow();
}

bool ShouldShowKind(ClickKind kind) {
    switch (kind) {
    case ClickKind::LeftDown:
        return gApp.settings.showPress;
    case ClickKind::LeftUp:
        return gApp.settings.showRelease;
    case ClickKind::RightDown:
    case ClickKind::RightUp:
        return gApp.settings.showRightClick;
    case ClickKind::Drag:
        return gApp.settings.showDrag;
    }
    return false;
}

bool AcceptRecentEvent(ClickKind kind, POINT point) {
    const ULONGLONG now = static_cast<ULONGLONG>(GetTickCount());
    gApp.recentEvents.erase(
        std::remove_if(gApp.recentEvents.begin(), gApp.recentEvents.end(), [now](const RecentEvent& event) {
            return now - event.tick > 100;
        }),
        gApp.recentEvents.end()
    );

    for (const RecentEvent& event : gApp.recentEvents) {
        if (event.kind == kind &&
            std::abs(event.point.x - point.x) < 3 &&
            std::abs(event.point.y - point.y) < 3) {
            return false;
        }
    }

    gApp.recentEvents.push_back(RecentEvent{kind, point, now});
    return true;
}

LRESULT CALLBACK PulseWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void RenderPulseWindow(PulseWindow* pulse) {
    if (!pulse || !pulse->hwnd) {
        return;
    }

    ULONGLONG now = static_cast<ULONGLONG>(GetTickCount());
    double progress = (now - pulse->startTick) / pulse->durationMs;
    if (progress >= 1.0) {
        DestroyWindow(pulse->hwnd);
        return;
    }

    progress = Clamp(progress, 0.0, 1.0);
    double eased = 1.0 - std::pow(1.0 - progress, 3.0);
    double fade = 1.0 - eased;
    double visualIntensity = Clamp(pulse->intensity, 0.15, 1.35);
    double alpha = Clamp(fade * (0.18 + visualIntensity * 0.78), 0.0, 1.0);
    double lineWidth = std::max(2.25, pulse->baseSize * (0.035 + visualIntensity * 0.045));

    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = pulse->bitmapSize;
    bi.bV5Height = -pulse->bitmapSize;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap = CreateDIBSection(screenDc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    Graphics graphics(memoryDc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));

    const float center = static_cast<float>(pulse->bitmapSize / 2.0);
    const float lineWidthF = static_cast<float>(lineWidth);

    auto strokeColor = [&](double a) {
        return Color(static_cast<BYTE>(Clamp(a, 0.0, 1.0) * 255.0), pulse->color.GetR(), pulse->color.GetG(), pulse->color.GetB());
    };
    auto ring = [&](float radius, float width, double a) {
        Pen pen(strokeColor(a), width);
        graphics.DrawEllipse(&pen, center - radius, center - radius, radius * 2.0f, radius * 2.0f);
    };
    auto dot = [&](float radius, double a) {
        SolidBrush brush(strokeColor(a));
        graphics.FillEllipse(&brush, center - radius, center - radius, radius * 2.0f, radius * 2.0f);
    };
    auto glow = [&](float radius, double a) {
        if (a <= 0.0) {
            return;
        }
        SolidBrush brush(strokeColor(a));
        graphics.FillEllipse(&brush, center - radius, center - radius, radius * 2.0f, radius * 2.0f);
    };
    auto crosshair = [&](float size, double a) {
        Pen pen(strokeColor(a), std::max(2.0f, size * 0.12f));
        graphics.DrawLine(&pen, center - size, center, center + size, center);
        graphics.DrawLine(&pen, center, center - size, center, center + size);
    };

    switch (pulse->kind) {
    case ClickKind::LeftDown: {
        if (visualIntensity >= 0.7) {
            double glowAlpha = Clamp(fade * visualIntensity * (visualIntensity >= 1.2 ? 0.18 : 0.08), 0.0, 1.0);
            glow(static_cast<float>(pulse->baseSize * (0.28 + 0.78 * eased)), glowAlpha);
        }
        ring(static_cast<float>(pulse->baseSize * (0.18 + 0.62 * eased)), lineWidthF, alpha);
        dot(static_cast<float>(pulse->baseSize * 0.085), alpha * 0.75);
        break;
    }
    case ClickKind::LeftUp: {
        double releaseRadius = pulse->baseSize * (0.76 - 0.42 * eased);
        double releaseAlpha = alpha * 0.55;
        if (visualIntensity >= 0.7) {
            glow(static_cast<float>(releaseRadius * 1.25), Clamp(fade * visualIntensity * 0.45, 0.0, 1.0));
        }
        ring(static_cast<float>(releaseRadius), lineWidthF * 0.55f, releaseAlpha);
        dot(static_cast<float>(pulse->baseSize * 0.055), releaseAlpha * 0.6);
        break;
    }
    case ClickKind::RightDown: {
        if (visualIntensity >= 0.7) {
            double glowAlpha = Clamp(fade * visualIntensity * (visualIntensity >= 1.2 ? 0.18 : 0.08), 0.0, 1.0);
            glow(static_cast<float>(pulse->baseSize * (0.28 + 0.7 * eased)), glowAlpha);
        }
        ring(static_cast<float>(pulse->baseSize * (0.18 + 0.54 * eased)), lineWidthF, alpha);
        crosshair(static_cast<float>(pulse->baseSize * 0.28), alpha * 0.85);
        break;
    }
    case ClickKind::RightUp: {
        double releaseRadius = pulse->baseSize * (0.68 - 0.36 * eased);
        double releaseAlpha = alpha * 0.5;
        if (visualIntensity >= 0.7) {
            glow(static_cast<float>(releaseRadius * 1.22), Clamp(fade * visualIntensity * 0.4, 0.0, 1.0));
        }
        ring(static_cast<float>(releaseRadius), lineWidthF * 0.55f, releaseAlpha);
        crosshair(static_cast<float>(pulse->baseSize * (0.16 + 0.08 * fade)), releaseAlpha * 0.7);
        break;
    }
    case ClickKind::Drag:
        dot(static_cast<float>(pulse->baseSize * (0.08 + 0.065 * visualIntensity)), alpha * 0.78);
        break;
    }

    SIZE windowSize = {pulse->bitmapSize, pulse->bitmapSize};
    POINT windowPosition = {pulse->screenPoint.x - (pulse->bitmapSize / 2), pulse->screenPoint.y - (pulse->bitmapSize / 2)};
    POINT sourcePoint = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(pulse->hwnd, screenDc, &windowPosition, &windowSize, memoryDc, &sourcePoint, 0, &blend, ULW_ALPHA);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
}

void RemovePulse(PulseWindow* pulse) {
    auto it = std::find(gApp.pulses.begin(), gApp.pulses.end(), pulse);
    if (it != gApp.pulses.end()) {
        gApp.pulses.erase(it);
    }
    delete pulse;
}

void ShowPulse(ClickKind kind, POINT point) {
    if (!gApp.settings.enabled || !ShouldShowKind(kind) || !AcceptRecentEvent(kind, point)) {
        return;
    }

    PulseWindow* pulse = new PulseWindow();
    pulse->kind = kind;
    pulse->screenPoint = point;
    pulse->startTick = static_cast<ULONGLONG>(GetTickCount());
    pulse->durationMs = EventDurationMs(gApp.settings, kind);
    pulse->baseSize = EventBaseSize(gApp.settings, kind);
    pulse->intensity = gApp.settings.intensity;
    pulse->color = EventColor(gApp.settings, kind);
    pulse->bitmapSize = static_cast<int>(std::ceil(std::max(64.0, pulse->baseSize * 2.7)));

    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kPulseClassName,
        L"",
        WS_POPUP,
        point.x - pulse->bitmapSize / 2,
        point.y - pulse->bitmapSize / 2,
        pulse->bitmapSize,
        pulse->bitmapSize,
        nullptr,
        nullptr,
        gApp.instance,
        pulse
    );

    if (!hwnd) {
        delete pulse;
        return;
    }

    pulse->hwnd = hwnd;
    gApp.pulses.push_back(pulse);

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetTimer(hwnd, kPulseTimerId, kPulseFrameMs, nullptr);
    RenderPulseWindow(pulse);
}

LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        POINT point = info->pt;
        switch (wParam) {
        case WM_LBUTTONDOWN:
            gApp.leftButtonDown = true;
            ShowPulse(ClickKind::LeftDown, point);
            break;
        case WM_LBUTTONUP:
            gApp.leftButtonDown = false;
            ShowPulse(ClickKind::LeftUp, point);
            break;
        case WM_RBUTTONDOWN:
            gApp.rightButtonDown = true;
            ShowPulse(ClickKind::RightDown, point);
            break;
        case WM_RBUTTONUP:
            gApp.rightButtonDown = false;
            ShowPulse(ClickKind::RightUp, point);
            break;
        case WM_MOUSEMOVE: {
            if (!gApp.settings.showDrag) {
                break;
            }
            if (!gApp.leftButtonDown && !gApp.rightButtonDown) {
                break;
            }
            ULONGLONG now = static_cast<ULONGLONG>(GetTickCount());
            int dx = point.x - gApp.lastDragPoint.x;
            int dy = point.y - gApp.lastDragPoint.y;
            if (now - gApp.lastDragTick >= 40 || (dx * dx + dy * dy) >= 144) {
                gApp.lastDragTick = now;
                gApp.lastDragPoint = point;
                ShowPulse(ClickKind::Drag, point);
            }
            break;
        }
        default:
            break;
        }
    }
    return CallNextHookEx(gApp.mouseHook, code, wParam, lParam);
}

void StartMouseHook() {
    if (gApp.mouseHook || !gApp.settings.enabled) {
        return;
    }
    gApp.mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, gApp.instance, 0);
}

void StopMouseHook() {
    if (gApp.mouseHook) {
        UnhookWindowsHookEx(gApp.mouseHook);
        gApp.mouseHook = nullptr;
    }
}

void RefreshHookState() {
    if (gApp.settings.enabled) {
        StartMouseHook();
    } else {
        StopMouseHook();
    }
}

std::wstring CaptureStatusText() {
    return gApp.mouseHook ? L"Click Capture: Low-level Hook" : L"Click Capture: Stopped";
}

int FindOptionIndex(const OptionItem* options, size_t count, double value) {
    for (size_t index = 0; index < count; ++index) {
        if (std::abs(options[index].value - value) < 0.01) {
            return static_cast<int>(index);
        }
    }
    return 0;
}

int FindColorIndex(ColorPreset preset) {
    for (size_t index = 0; index < ArrayCount(kColorOptions); ++index) {
        if (kColorOptions[index].preset == preset) {
            return static_cast<int>(index);
        }
    }
    return 0;
}

void SetComboItems(HWND combo, const OptionItem* options, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(options[index].label));
    }
}

void SetColorComboItems(HWND combo) {
    for (size_t index = 0; index < ArrayCount(kColorOptions); ++index) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kColorOptions[index].label));
    }
}

void SetCheckboxState(int controlId, bool checked) {
    if (!gApp.settingsWindow) {
        return;
    }
    SendMessageW(GetDlgItem(gApp.settingsWindow, controlId), BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SetComboSelection(int controlId, int index) {
    if (!gApp.settingsWindow) {
        return;
    }
    SendMessageW(GetDlgItem(gApp.settingsWindow, controlId), CB_SETCURSEL, index, 0);
}

void SyncSettingsWindow() {
    if (!gApp.settingsWindow) {
        return;
    }

    SetCheckboxState(CID_ENABLED, gApp.settings.enabled);
    SetCheckboxState(CID_SHOW_PRESS, gApp.settings.showPress);
    SetCheckboxState(CID_SHOW_RELEASE, gApp.settings.showRelease);
    SetCheckboxState(CID_SHOW_RIGHT, gApp.settings.showRightClick);
    SetCheckboxState(CID_SHOW_DRAG, gApp.settings.showDrag);
    SetCheckboxState(CID_COMPACT_ICON, gApp.settings.compactTrayIcon);
    SetComboSelection(CID_SIZE, FindOptionIndex(kSizeOptions, ArrayCount(kSizeOptions), gApp.settings.size));
    SetComboSelection(CID_INTENSITY, FindOptionIndex(kIntensityOptions, ArrayCount(kIntensityOptions), gApp.settings.intensity));
    SetComboSelection(CID_DURATION, FindOptionIndex(kDurationOptions, ArrayCount(kDurationOptions), gApp.settings.duration));
    SetComboSelection(CID_COLOR, FindColorIndex(gApp.settings.colorPreset));
}

void CreateSettingsControls(HWND hwnd) {
    CreateWindowExW(0, L"STATIC", L"ClickBait Settings", WS_CHILD | WS_VISIBLE, 18, 16, 220, 22, hwnd, nullptr, gApp.instance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Enable click highlights", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 48, 220, 24, hwnd, reinterpret_cast<HMENU>(CID_ENABLED), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Show press", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 78, 160, 24, hwnd, reinterpret_cast<HMENU>(CID_SHOW_PRESS), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Show release", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 104, 160, 24, hwnd, reinterpret_cast<HMENU>(CID_SHOW_RELEASE), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Show right click", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 130, 160, 24, hwnd, reinterpret_cast<HMENU>(CID_SHOW_RIGHT), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Show drag", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 156, 160, 24, hwnd, reinterpret_cast<HMENU>(CID_SHOW_DRAG), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Compact tray icon", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 18, 182, 180, 24, hwnd, reinterpret_cast<HMENU>(CID_COMPACT_ICON), gApp.instance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Size", WS_CHILD | WS_VISIBLE, 18, 222, 80, 20, hwnd, nullptr, gApp.instance, nullptr);
    HWND sizeCombo = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS, 18, 244, 150, 160, hwnd, reinterpret_cast<HMENU>(CID_SIZE), gApp.instance, nullptr);
    SetComboItems(sizeCombo, kSizeOptions, ArrayCount(kSizeOptions));

    CreateWindowExW(0, L"STATIC", L"Intensity", WS_CHILD | WS_VISIBLE, 196, 222, 80, 20, hwnd, nullptr, gApp.instance, nullptr);
    HWND intensityCombo = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS, 196, 244, 150, 160, hwnd, reinterpret_cast<HMENU>(CID_INTENSITY), gApp.instance, nullptr);
    SetComboItems(intensityCombo, kIntensityOptions, ArrayCount(kIntensityOptions));

    CreateWindowExW(0, L"STATIC", L"Duration", WS_CHILD | WS_VISIBLE, 18, 286, 80, 20, hwnd, nullptr, gApp.instance, nullptr);
    HWND durationCombo = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS, 18, 308, 150, 160, hwnd, reinterpret_cast<HMENU>(CID_DURATION), gApp.instance, nullptr);
    SetComboItems(durationCombo, kDurationOptions, ArrayCount(kDurationOptions));

    CreateWindowExW(0, L"STATIC", L"Color", WS_CHILD | WS_VISIBLE, 196, 286, 80, 20, hwnd, nullptr, gApp.instance, nullptr);
    HWND colorCombo = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS, 196, 308, 150, 180, hwnd, reinterpret_cast<HMENU>(CID_COLOR), gApp.instance, nullptr);
    SetColorComboItems(colorCombo);

    CreateWindowExW(0, L"BUTTON", L"Test Pulse", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 18, 360, 110, 30, hwnd, reinterpret_cast<HMENU>(CID_TEST), gApp.instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Minimize", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 236, 360, 110, 30, hwnd, reinterpret_cast<HMENU>(CID_CLOSE), gApp.instance, nullptr);
}

LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateSettingsControls(hwnd);
        SyncSettingsWindow();
        return 0;
    case WM_COMMAND: {
        const int controlId = LOWORD(wParam);
        const int notifyCode = HIWORD(wParam);
        switch (controlId) {
        case CID_ENABLED:
            gApp.settings.enabled = SendMessageW(GetDlgItem(hwnd, CID_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged(true, true);
            return 0;
        case CID_SHOW_PRESS:
            gApp.settings.showPress = SendMessageW(GetDlgItem(hwnd, CID_SHOW_PRESS), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged();
            return 0;
        case CID_SHOW_RELEASE:
            gApp.settings.showRelease = SendMessageW(GetDlgItem(hwnd, CID_SHOW_RELEASE), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged();
            return 0;
        case CID_SHOW_RIGHT:
            gApp.settings.showRightClick = SendMessageW(GetDlgItem(hwnd, CID_SHOW_RIGHT), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged();
            return 0;
        case CID_SHOW_DRAG:
            gApp.settings.showDrag = SendMessageW(GetDlgItem(hwnd, CID_SHOW_DRAG), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged();
            return 0;
        case CID_COMPACT_ICON:
            gApp.settings.compactTrayIcon = SendMessageW(GetDlgItem(hwnd, CID_COMPACT_ICON), BM_GETCHECK, 0, 0) == BST_CHECKED;
            ApplySettingsChanged(false, true);
            return 0;
        case CID_SIZE:
            if (notifyCode == CBN_SELCHANGE) {
                int index = static_cast<int>(SendMessageW(GetDlgItem(hwnd, CID_SIZE), CB_GETCURSEL, 0, 0));
                if (index >= 0 && static_cast<size_t>(index) < ArrayCount(kSizeOptions)) {
                    gApp.settings.size = kSizeOptions[index].value;
                    ApplySettingsChanged();
                }
            }
            return 0;
        case CID_INTENSITY:
            if (notifyCode == CBN_SELCHANGE) {
                int index = static_cast<int>(SendMessageW(GetDlgItem(hwnd, CID_INTENSITY), CB_GETCURSEL, 0, 0));
                if (index >= 0 && static_cast<size_t>(index) < ArrayCount(kIntensityOptions)) {
                    gApp.settings.intensity = kIntensityOptions[index].value;
                    ApplySettingsChanged();
                }
            }
            return 0;
        case CID_DURATION:
            if (notifyCode == CBN_SELCHANGE) {
                int index = static_cast<int>(SendMessageW(GetDlgItem(hwnd, CID_DURATION), CB_GETCURSEL, 0, 0));
                if (index >= 0 && static_cast<size_t>(index) < ArrayCount(kDurationOptions)) {
                    gApp.settings.duration = kDurationOptions[index].value;
                    ApplySettingsChanged();
                }
            }
            return 0;
        case CID_COLOR:
            if (notifyCode == CBN_SELCHANGE) {
                int index = static_cast<int>(SendMessageW(GetDlgItem(hwnd, CID_COLOR), CB_GETCURSEL, 0, 0));
                if (index >= 0 && static_cast<size_t>(index) < ArrayCount(kColorOptions)) {
                    gApp.settings.colorPreset = kColorOptions[index].preset;
                    ApplySettingsChanged();
                }
            }
            return 0;
        case CID_TEST: {
            POINT point;
            GetCursorPos(&point);
            ShowPulse(ClickKind::LeftDown, point);
            return 0;
        }
        case CID_CLOSE:
            ShowWindow(hwnd, SW_MINIMIZE);
            return 0;
        default:
            return 0;
        }
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_MINIMIZE);
        return 0;
    case WM_DESTROY:
        gApp.settingsWindow = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void EnsureSettingsWindow() {
    if (gApp.settingsWindow) {
        return;
    }

    gApp.settingsWindow = CreateWindowExW(
        WS_EX_APPWINDOW,
        kSettingsClassName,
        L"ClickBait Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        382,
        450,
        gApp.mainWindow,
        nullptr,
        gApp.instance,
        nullptr
    );
}

void ShowSettingsWindow() {
    EnsureSettingsWindow();
    if (!gApp.settingsWindow) {
        return;
    }
    SyncSettingsWindow();
    ShowWindow(gApp.settingsWindow, IsIconic(gApp.settingsWindow) ? SW_RESTORE : SW_SHOWNORMAL);
    SetForegroundWindow(gApp.settingsWindow);
}

void ToggleSetting(bool& value) {
    value = !value;
    SyncSettingsWindow();
}

void SetCheckedState(HMENU menu, UINT id, bool isChecked) {
    CheckMenuItem(menu, id, MF_BYCOMMAND | (isChecked ? MF_CHECKED : MF_UNCHECKED));
}

HMENU BuildContextMenu() {
    HMENU menu = CreatePopupMenu();
    HMENU sizeMenu = CreatePopupMenu();
    HMENU intensityMenu = CreatePopupMenu();
    HMENU durationMenu = CreatePopupMenu();
    HMENU colorMenu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING, ID_TOGGLE_ENABLED, L"Enabled");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_PRESS, L"Show Press");
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_RELEASE, L"Show Release");
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_RIGHT_CLICK, L"Show Right Click");
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_DRAG, L"Show Drag");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TOGGLE_COMPACT_ICON, L"Compact Tray Icon");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(sizeMenu, MF_STRING, ID_SIZE_SMALL, L"Small");
    AppendMenuW(sizeMenu, MF_STRING, ID_SIZE_MEDIUM, L"Medium");
    AppendMenuW(sizeMenu, MF_STRING, ID_SIZE_LARGE, L"Large");
    AppendMenuW(sizeMenu, MF_STRING, ID_SIZE_HUGE, L"Huge");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sizeMenu), L"Size");

    AppendMenuW(intensityMenu, MF_STRING, ID_INTENSITY_SUBTLE, L"Subtle");
    AppendMenuW(intensityMenu, MF_STRING, ID_INTENSITY_NORMAL, L"Normal");
    AppendMenuW(intensityMenu, MF_STRING, ID_INTENSITY_BRIGHT, L"Bright");
    AppendMenuW(intensityMenu, MF_STRING, ID_INTENSITY_BEACON, L"Beacon");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(intensityMenu), L"Intensity");

    AppendMenuW(durationMenu, MF_STRING, ID_DURATION_SNAPPY, L"Snappy");
    AppendMenuW(durationMenu, MF_STRING, ID_DURATION_NORMAL, L"Normal");
    AppendMenuW(durationMenu, MF_STRING, ID_DURATION_SLOW, L"Slow");
    AppendMenuW(durationMenu, MF_STRING, ID_DURATION_VERY_SLOW, L"Very Slow");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(durationMenu), L"Duration");

    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_DEFAULT, L"Default");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_BLUE, L"Blue");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_GREEN, L"Green");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_PURPLE, L"Purple");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_PINK, L"Pink");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_ORANGE, L"Orange");
    AppendMenuW(colorMenu, MF_STRING, ID_COLOR_WHITE, L"White");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colorMenu), L"Colors");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_GRAYED, 0, CaptureStatusText().c_str());
    AppendMenuW(menu, MF_STRING, ID_OPEN_SETTINGS, L"Open Settings...");
    AppendMenuW(menu, MF_STRING, ID_TEST_PULSE, L"Test Pulse at Pointer");
    AppendMenuW(menu, MF_GRAYED, 0, L"Permissions: Not required on Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_CHECK_UPDATES, L"Check for Updates...");
    AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit ClickBait");

    SetCheckedState(menu, ID_TOGGLE_ENABLED, gApp.settings.enabled);
    SetCheckedState(menu, ID_TOGGLE_PRESS, gApp.settings.showPress);
    SetCheckedState(menu, ID_TOGGLE_RELEASE, gApp.settings.showRelease);
    SetCheckedState(menu, ID_TOGGLE_RIGHT_CLICK, gApp.settings.showRightClick);
    SetCheckedState(menu, ID_TOGGLE_DRAG, gApp.settings.showDrag);
    SetCheckedState(menu, ID_TOGGLE_COMPACT_ICON, gApp.settings.compactTrayIcon);

    const double size = gApp.settings.size;
    SetCheckedState(sizeMenu, ID_SIZE_SMALL, std::abs(size - 44.0) < 0.01);
    SetCheckedState(sizeMenu, ID_SIZE_MEDIUM, std::abs(size - 64.0) < 0.01);
    SetCheckedState(sizeMenu, ID_SIZE_LARGE, std::abs(size - 88.0) < 0.01);
    SetCheckedState(sizeMenu, ID_SIZE_HUGE, std::abs(size - 116.0) < 0.01);

    const double intensity = gApp.settings.intensity;
    SetCheckedState(intensityMenu, ID_INTENSITY_SUBTLE, std::abs(intensity - 0.28) < 0.01);
    SetCheckedState(intensityMenu, ID_INTENSITY_NORMAL, std::abs(intensity - 0.70) < 0.01);
    SetCheckedState(intensityMenu, ID_INTENSITY_BRIGHT, std::abs(intensity - 1.00) < 0.01);
    SetCheckedState(intensityMenu, ID_INTENSITY_BEACON, std::abs(intensity - 1.35) < 0.01);

    const double duration = gApp.settings.duration;
    SetCheckedState(durationMenu, ID_DURATION_SNAPPY, std::abs(duration - 0.28) < 0.01);
    SetCheckedState(durationMenu, ID_DURATION_NORMAL, std::abs(duration - 0.48) < 0.01);
    SetCheckedState(durationMenu, ID_DURATION_SLOW, std::abs(duration - 0.72) < 0.01);
    SetCheckedState(durationMenu, ID_DURATION_VERY_SLOW, std::abs(duration - 1.00) < 0.01);

    SetCheckedState(colorMenu, ID_COLOR_DEFAULT, gApp.settings.colorPreset == ColorPreset::Default);
    SetCheckedState(colorMenu, ID_COLOR_BLUE, gApp.settings.colorPreset == ColorPreset::Blue);
    SetCheckedState(colorMenu, ID_COLOR_GREEN, gApp.settings.colorPreset == ColorPreset::Green);
    SetCheckedState(colorMenu, ID_COLOR_PURPLE, gApp.settings.colorPreset == ColorPreset::Purple);
    SetCheckedState(colorMenu, ID_COLOR_PINK, gApp.settings.colorPreset == ColorPreset::Pink);
    SetCheckedState(colorMenu, ID_COLOR_ORANGE, gApp.settings.colorPreset == ColorPreset::Orange);
    SetCheckedState(colorMenu, ID_COLOR_WHITE, gApp.settings.colorPreset == ColorPreset::White);

    return menu;
}

void ShowContextMenuAt(const POINT* anchor = nullptr) {
    HMENU menu = BuildContextMenu();
    POINT point;
    if (anchor) {
        point = *anchor;
    } else {
        GetCursorPos(&point);
    }
    SetForegroundWindow(gApp.mainWindow);
    UINT selected = TrackPopupMenu(
        menu,
        TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
        point.x,
        point.y,
        0,
        gApp.mainWindow,
        nullptr
    );
    if (selected != 0) {
        PostMessageW(gApp.mainWindow, WM_COMMAND, MAKEWPARAM(selected, 0), 0);
    }
    PostMessageW(gApp.mainWindow, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void HandleTrayNotification(WPARAM wParam, LPARAM lParam) {
    UINT eventCode = LOWORD(static_cast<DWORD>(lParam));
    if (eventCode == 0) {
        eventCode = static_cast<UINT>(lParam);
    }

    POINT anchor{};
    bool hasAnchor = false;
    if (eventCode == WM_CONTEXTMENU) {
        if (GetCursorPos(&anchor)) {
            hasAnchor = true;
        }
    }

    switch (eventCode) {
    case NIN_SELECT:
    case NIN_KEYSELECT:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        ShowSettingsWindow();
        break;
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_CONTEXTMENU:
        ShowContextMenuAt(hasAnchor ? &anchor : nullptr);
        break;
    default:
        break;
    }
}

void SetColorPreset(ColorPreset preset) {
    gApp.settings.colorPreset = preset;
    ApplySettingsChanged();
}

void HandleCommand(UINT id) {
    switch (id) {
    case ID_TOGGLE_ENABLED:
        ToggleSetting(gApp.settings.enabled);
        ApplySettingsChanged(true, true);
        break;
    case ID_TOGGLE_PRESS:
        ToggleSetting(gApp.settings.showPress);
        ApplySettingsChanged();
        break;
    case ID_TOGGLE_RELEASE:
        ToggleSetting(gApp.settings.showRelease);
        ApplySettingsChanged();
        break;
    case ID_TOGGLE_RIGHT_CLICK:
        ToggleSetting(gApp.settings.showRightClick);
        ApplySettingsChanged();
        break;
    case ID_TOGGLE_DRAG:
        ToggleSetting(gApp.settings.showDrag);
        ApplySettingsChanged();
        break;
    case ID_TOGGLE_COMPACT_ICON:
        ToggleSetting(gApp.settings.compactTrayIcon);
        ApplySettingsChanged(false, true);
        break;
    case ID_SIZE_SMALL:
        gApp.settings.size = 44.0;
        ApplySettingsChanged();
        break;
    case ID_SIZE_MEDIUM:
        gApp.settings.size = 64.0;
        ApplySettingsChanged();
        break;
    case ID_SIZE_LARGE:
        gApp.settings.size = 88.0;
        ApplySettingsChanged();
        break;
    case ID_SIZE_HUGE:
        gApp.settings.size = 116.0;
        ApplySettingsChanged();
        break;
    case ID_INTENSITY_SUBTLE:
        gApp.settings.intensity = 0.28;
        ApplySettingsChanged();
        break;
    case ID_INTENSITY_NORMAL:
        gApp.settings.intensity = 0.70;
        ApplySettingsChanged();
        break;
    case ID_INTENSITY_BRIGHT:
        gApp.settings.intensity = 1.0;
        ApplySettingsChanged();
        break;
    case ID_INTENSITY_BEACON:
        gApp.settings.intensity = 1.35;
        ApplySettingsChanged();
        break;
    case ID_DURATION_SNAPPY:
        gApp.settings.duration = 0.28;
        ApplySettingsChanged();
        break;
    case ID_DURATION_NORMAL:
        gApp.settings.duration = 0.48;
        ApplySettingsChanged();
        break;
    case ID_DURATION_SLOW:
        gApp.settings.duration = 0.72;
        ApplySettingsChanged();
        break;
    case ID_DURATION_VERY_SLOW:
        gApp.settings.duration = 1.0;
        ApplySettingsChanged();
        break;
    case ID_COLOR_DEFAULT:
        SetColorPreset(ColorPreset::Default);
        break;
    case ID_COLOR_BLUE:
        SetColorPreset(ColorPreset::Blue);
        break;
    case ID_COLOR_GREEN:
        SetColorPreset(ColorPreset::Green);
        break;
    case ID_COLOR_PURPLE:
        SetColorPreset(ColorPreset::Purple);
        break;
    case ID_COLOR_PINK:
        SetColorPreset(ColorPreset::Pink);
        break;
    case ID_COLOR_ORANGE:
        SetColorPreset(ColorPreset::Orange);
        break;
    case ID_COLOR_WHITE:
        SetColorPreset(ColorPreset::White);
        break;
    case ID_OPEN_SETTINGS:
        ShowSettingsWindow();
        break;
    case ID_TEST_PULSE: {
        POINT point;
        GetCursorPos(&point);
        ShowPulse(ClickKind::LeftDown, point);
        break;
    }
    case ID_CHECK_UPDATES:
        ShellExecuteW(nullptr, L"open", kRepoUrl, nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_QUIT:
        PostMessageW(gApp.mainWindow, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        HandleCommand(LOWORD(wParam));
        return 0;
    case kTrayMessage:
        HandleTrayNotification(wParam, lParam);
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &gApp.trayIcon);
        StopMouseHook();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK PulseWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PulseWindow* pulse = reinterpret_cast<PulseWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_TIMER:
        if (pulse && wParam == kPulseTimerId) {
            RenderPulseWindow(pulse);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_NCDESTROY:
        if (pulse) {
            KillTimer(hwnd, kPulseTimerId);
            RemovePulse(pulse);
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

bool RegisterWindowClasses() {
    HICON appIcon = static_cast<HICON>(LoadImageW(
        gApp.instance,
        MAKEINTRESOURCEW(kAppIconResourceId),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE
    ));
    HICON smallIcon = static_cast<HICON>(LoadImageW(
        gApp.instance,
        MAKEINTRESOURCEW(kAppIconResourceId),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        0
    ));

    WNDCLASSEXW mainClass = {};
    mainClass.cbSize = sizeof(WNDCLASSEXW);
    mainClass.hInstance = gApp.instance;
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.lpszClassName = kAppClassName;
    mainClass.hIcon = appIcon;
    mainClass.hIconSm = smallIcon;
    if (!RegisterClassExW(&mainClass)) {
        return false;
    }

    WNDCLASSEXW pulseClass = {};
    pulseClass.cbSize = sizeof(WNDCLASSEXW);
    pulseClass.hInstance = gApp.instance;
    pulseClass.lpfnWndProc = PulseWindowProc;
    pulseClass.lpszClassName = kPulseClassName;
    pulseClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&pulseClass)) {
        return false;
    }

    WNDCLASSEXW settingsClass = {};
    settingsClass.cbSize = sizeof(WNDCLASSEXW);
    settingsClass.hInstance = gApp.instance;
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.lpszClassName = kSettingsClassName;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    settingsClass.hIcon = appIcon;
    settingsClass.hIconSm = smallIcon;
    if (!RegisterClassExW(&settingsClass)) {
        return false;
    }

    return true;
}

bool CreateMainWindow() {
    gApp.mainWindow = CreateWindowExW(
        0,
        kAppClassName,
        L"ClickBait",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr,
        nullptr,
        gApp.instance,
        nullptr
    );
    return gApp.mainWindow != nullptr;
}

bool CreateTrayIcon() {
    gApp.activeRegularIcon = BuildTrayIcon(false, true);
    gApp.activeCompactIcon = BuildTrayIcon(true, true);
    gApp.inactiveRegularIcon = BuildTrayIcon(false, false);
    gApp.inactiveCompactIcon = BuildTrayIcon(true, false);

    gApp.trayIcon = {};
    gApp.trayIcon.cbSize = sizeof(NOTIFYICONDATAW);
    gApp.trayIcon.hWnd = gApp.mainWindow;
    gApp.trayIcon.uID = kTrayIconId;
    gApp.trayIcon.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    gApp.trayIcon.uCallbackMessage = kTrayMessage;
    gApp.trayIcon.hIcon = gApp.settings.enabled
        ? (gApp.settings.compactTrayIcon ? gApp.activeCompactIcon : gApp.activeRegularIcon)
        : (gApp.settings.compactTrayIcon ? gApp.inactiveCompactIcon : gApp.inactiveRegularIcon);
    lstrcpynW(gApp.trayIcon.szTip, gApp.settings.enabled ? L"ClickBait (On)" : L"ClickBait (Off)", ARRAYSIZE(gApp.trayIcon.szTip));

    return Shell_NotifyIconW(NIM_ADD, &gApp.trayIcon) == TRUE;
}

void CleanupIcons() {
    if (gApp.activeRegularIcon) {
        DestroyIcon(gApp.activeRegularIcon);
        gApp.activeRegularIcon = nullptr;
    }
    if (gApp.activeCompactIcon) {
        DestroyIcon(gApp.activeCompactIcon);
        gApp.activeCompactIcon = nullptr;
    }
    if (gApp.inactiveRegularIcon) {
        DestroyIcon(gApp.inactiveRegularIcon);
        gApp.inactiveRegularIcon = nullptr;
    }
    if (gApp.inactiveCompactIcon) {
        DestroyIcon(gApp.inactiveCompactIcon);
        gApp.inactiveCompactIcon = nullptr;
    }
}

bool InitializeGdiplus() {
    GdiplusStartupInput input;
    return GdiplusStartup(&gApp.gdiplusToken, &input, nullptr) == Ok;
}

void InitializeDpiAwareness() {
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);
        auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext")
        );
        if (setContext) {
            const HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(-4);
            if (setContext(perMonitorV2)) {
                FreeLibrary(user32);
                return;
            }
        }

        typedef BOOL (WINAPI *SetProcessDPIAwareFn)();
        auto setAware = reinterpret_cast<SetProcessDPIAwareFn>(
            GetProcAddress(user32, "SetProcessDPIAware")
        );
        if (setAware) {
            setAware();
        }
        FreeLibrary(user32);
    }
}

void ShutdownGdiplus() {
    if (gApp.gdiplusToken != 0) {
        GdiplusShutdown(gApp.gdiplusToken);
        gApp.gdiplusToken = 0;
    }
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    gApp.instance = instance;
    InitializeDpiAwareness();
    LoadSettings();

    if (!InitializeGdiplus()) {
        MessageBoxW(nullptr, L"ClickBait could not start GDI+ rendering.", L"ClickBait", MB_OK | MB_ICONERROR);
        return 11;
    }

    if (!RegisterWindowClasses()) {
        CleanupIcons();
        ShutdownGdiplus();
        MessageBoxW(nullptr, L"ClickBait could not register its window classes.", L"ClickBait", MB_OK | MB_ICONERROR);
        return 12;
    }

    if (!CreateMainWindow()) {
        CleanupIcons();
        ShutdownGdiplus();
        MessageBoxW(nullptr, L"ClickBait could not create its background window.", L"ClickBait", MB_OK | MB_ICONERROR);
        return 13;
    }

    if (!CreateTrayIcon()) {
        MessageBoxW(
            nullptr,
            L"ClickBait could not create its tray icon. The settings window will still open so you can use the app.",
            L"ClickBait",
            MB_OK | MB_ICONWARNING
        );
    }

    RefreshHookState();
    ShowSettingsWindow();

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CleanupIcons();
    ShutdownGdiplus();
    return static_cast<int>(message.wParam);
}
