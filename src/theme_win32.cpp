#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_WINDOWS)

#include "xpgui/theme.h"
#include "xpgui/log.h"

#include <atomic>
#include <mutex>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace xpgui {

namespace {

// ---------------------------------------------------------------------------
// Undocumented uxtheme.dll dark-mode APIs (Windows 10 1809+).
// Ordinals are stable across builds, but the entry points are not part of
// the public SDK and may be removed in future Windows versions.
// ---------------------------------------------------------------------------

using fnSetPreferredAppMode             = int  (WINAPI*)(int mode);
using fnAllowDarkModeForWindow          = bool (WINAPI*)(HWND hwnd, bool allow);
using fnRefreshImmersiveColorPolicyState = void (WINAPI*)();
using fnShouldAppsUseDarkMode           = bool (WINAPI*)();

struct DarkModeAPIs {
    fnSetPreferredAppMode setPreferredAppMode = nullptr;
    fnAllowDarkModeForWindow allowDarkModeForWindow = nullptr;
    fnRefreshImmersiveColorPolicyState refreshImmersiveColorPolicyState = nullptr;
    fnShouldAppsUseDarkMode shouldAppsUseDarkMode = nullptr;
    bool loaded = false;
    bool usable = false;
};

DarkModeAPIs& apis() {
    static DarkModeAPIs s;
    return s;
}

void ensureLoaded() {
    auto& a = apis();
    if (a.loaded) return;
    a.loaded = true;

    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) {
        xpgui::log::warn("uxtheme.dll not available — dark mode disabled");
        return;
    }
    a.setPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
    a.allowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133)));
    a.refreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104)));
    a.shouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(132)));

    a.usable = a.setPreferredAppMode && a.allowDarkModeForWindow;
    if (a.usable) {
        // 1 = AllowDark. Idempotent across calls.
        a.setPreferredAppMode(1);
        if (a.refreshImmersiveColorPolicyState) {
            a.refreshImmersiveColorPolicyState();
        }
        xpgui::log::debug("Dark mode APIs loaded");
    } else {
        xpgui::log::warn("Dark mode APIs missing required ordinals — fallback to light");
    }
}

bool readRegistryDarkMode() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1, size = sizeof(value);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(&value), &size);
        RegCloseKey(hKey);
        return value == 0;  // 0 = dark mode
    }
    return false;
}

void setDarkTitleBar(HWND hwnd, bool dark) {
    BOOL useDark = dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 on Windows 10 20H1+
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
}

BOOL CALLBACK applyThemeToChild(HWND hwnd, LPARAM lp) {
    bool dark = lp != 0;
    auto& a = apis();
    if (a.allowDarkModeForWindow) {
        a.allowDarkModeForWindow(hwnd, dark);
    }
    // Controls flagged "_noTheme" (radio buttons/checkboxes inside groupboxes)
    // need classic rendering so WM_CTLCOLORSTATIC brushes are respected.
    if (GetPropW(hwnd, L"_noTheme")) {
        SetWindowTheme(hwnd, L"", L"");
        return TRUE;
    }
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    return TRUE;
}

// Static palettes — identical to HomeDrive's pre-extraction DARK_THEME /
// LIGHT_THEME constants so the visual diff is exactly zero.
const theme::Colors& darkPalette() {
    static const theme::Colors c = {
        theme::Color::fromCOLORREF(RGB(32, 32, 32)),    // bg
        theme::Color::fromCOLORREF(RGB(43, 43, 43)),    // panelBg
        theme::Color::fromCOLORREF(RGB(43, 43, 43)),    // editBg
        theme::Color::fromCOLORREF(RGB(230, 230, 230)), // text
        theme::Color::fromCOLORREF(RGB(80, 80, 80)),    // border
        theme::Color::fromCOLORREF(RGB(120, 120, 120)), // borderFocus
        theme::Color::fromCOLORREF(RGB(70, 70, 70)),    // btnPressed
        theme::Color::fromCOLORREF(RGB(55, 55, 80)),    // comboSel
    };
    return c;
}

const theme::Colors& lightPalette() {
    static const theme::Colors c = {
        theme::Color::fromCOLORREF(RGB(243, 243, 243)), // bg
        theme::Color::fromCOLORREF(RGB(255, 255, 255)), // panelBg
        theme::Color::fromCOLORREF(RGB(255, 255, 255)), // editBg
        theme::Color::fromCOLORREF(RGB(0, 0, 0)),       // text
        theme::Color::fromCOLORREF(RGB(200, 200, 200)), // border
        theme::Color::fromCOLORREF(RGB(100, 100, 100)), // borderFocus
        theme::Color::fromCOLORREF(RGB(220, 220, 220)), // btnPressed
        theme::Color::fromCOLORREF(RGB(200, 210, 240)), // comboSel
    };
    return c;
}

// ---------------------------------------------------------------------------
// Theme-change callback registry
// ---------------------------------------------------------------------------

struct CallbackEntry {
    win32::ThemeChangeToken token;
    std::function<void(theme::Mode)> cb;
};

std::mutex& registryMutex() {
    static std::mutex m;
    return m;
}

std::vector<CallbackEntry>& registry() {
    static std::vector<CallbackEntry> v;
    return v;
}

std::atomic<win32::ThemeChangeToken>& nextToken() {
    static std::atomic<win32::ThemeChangeToken> t{1};
    return t;
}

}  // namespace

// ---------------------------------------------------------------------------
// theme namespace implementation
// ---------------------------------------------------------------------------

namespace theme {

Mode systemMode() {
    ensureLoaded();
    auto& a = apis();
    bool dark = a.shouldAppsUseDarkMode ? a.shouldAppsUseDarkMode() : readRegistryDarkMode();
    return dark ? Mode::Dark : Mode::Light;
}

bool isHighContrast() {
    HIGHCONTRASTW hc = {};
    hc.cbSize = sizeof(hc);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    return false;
}

void applyMode(void* nativeWindowHandle, Mode mode) {
    if (!nativeWindowHandle) return;
    ensureLoaded();
    HWND hwnd = reinterpret_cast<HWND>(nativeWindowHandle);

    const bool dark = (mode == Mode::Dark);
    setDarkTitleBar(hwnd, dark);

    auto& a = apis();
    if (a.allowDarkModeForWindow) {
        a.allowDarkModeForWindow(hwnd, dark);
    }
    EnumChildWindows(hwnd, applyThemeToChild, dark ? 1 : 0);

    // Force full repaint so the themed background takes effect.
    InvalidateRect(hwnd, nullptr, TRUE);
}

void setWindowBorderColor(void* nativeWindowHandle, Color color) {
    if (!nativeWindowHandle) return;
    HWND hwnd = reinterpret_cast<HWND>(nativeWindowHandle);
    // DWMWA_BORDER_COLOR = 34 (Win11 22H2+). Older Windows: no-op.
    COLORREF cr = color.toCOLORREF();
    DwmSetWindowAttribute(hwnd, 34, &cr, sizeof(cr));
}

const Colors& defaultPalette(Mode mode) {
    return mode == Mode::Dark ? darkPalette() : lightPalette();
}

}  // namespace theme

// ---------------------------------------------------------------------------
// win32 namespace implementation
// ---------------------------------------------------------------------------

namespace win32 {

ThemeChangeToken onSystemThemeChanged(std::function<void(theme::Mode)> cb) {
    if (!cb) return 0;
    ThemeChangeToken tok = nextToken().fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(registryMutex());
    registry().push_back({ tok, std::move(cb) });
    return tok;
}

void offSystemThemeChanged(ThemeChangeToken token) {
    if (token == 0) return;
    std::lock_guard<std::mutex> lock(registryMutex());
    auto& vec = registry();
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        if (it->token == token) {
            vec.erase(it);
            return;
        }
    }
}

void notifySystemThemeChanged(theme::Mode newMode) {
    ensureLoaded();
    auto& a = apis();
    if (a.refreshImmersiveColorPolicyState) {
        a.refreshImmersiveColorPolicyState();
    }
    std::vector<std::function<void(theme::Mode)>> snapshot;
    {
        std::lock_guard<std::mutex> lock(registryMutex());
        snapshot.reserve(registry().size());
        for (auto& e : registry()) snapshot.push_back(e.cb);
    }
    for (auto& cb : snapshot) cb(newMode);
}

// ---- ThemeBrushes ----

ThemeBrushes::ThemeBrushes(const theme::Colors& colors) {
    recreate(colors);
}

ThemeBrushes::~ThemeBrushes() {
    if (hBgBrush_) DeleteObject(reinterpret_cast<HBRUSH>(hBgBrush_));
    if (hEditBgBrush_) DeleteObject(reinterpret_cast<HBRUSH>(hEditBgBrush_));
}

void* ThemeBrushes::bg() const { return hBgBrush_; }
void* ThemeBrushes::editBg() const { return hEditBgBrush_; }

void ThemeBrushes::recreate(const theme::Colors& colors) {
    if (hBgBrush_) {
        DeleteObject(reinterpret_cast<HBRUSH>(hBgBrush_));
        hBgBrush_ = nullptr;
    }
    if (hEditBgBrush_) {
        DeleteObject(reinterpret_cast<HBRUSH>(hEditBgBrush_));
        hEditBgBrush_ = nullptr;
    }
    hBgBrush_ = CreateSolidBrush(colors.bg.toCOLORREF());
    hEditBgBrush_ = CreateSolidBrush(colors.editBg.toCOLORREF());
}

}  // namespace win32

}  // namespace xpgui

#endif  // XPGUI_PLATFORM_WINDOWS
