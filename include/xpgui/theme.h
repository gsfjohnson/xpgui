#pragma once

#include <cstdint>
#include <functional>

#include "xpgui/platform.h"

namespace xpgui {

namespace theme {

enum class Mode { Light, Dark };

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    static constexpr Color fromCOLORREF(uint32_t cr) {
        return { uint8_t(cr & 0xFF),
                 uint8_t((cr >> 8) & 0xFF),
                 uint8_t((cr >> 16) & 0xFF) };
    }
    constexpr uint32_t toCOLORREF() const {
        return uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16);
    }
};

struct Colors {
    Color bg;
    Color panelBg;
    Color editBg;
    Color text;
    Color border;
    Color borderFocus;
    Color btnPressed;
    Color comboSel;
};

Mode systemMode();

inline bool systemUsesDarkMode() { return systemMode() == Mode::Dark; }

// True when the OS high-contrast accessibility mode is active.
// Windows: SPI_GETHIGHCONTRAST. macOS: NSWorkspace
// accessibilityDisplayShouldIncreaseContrast. Linux: GtkSettings
// gtk-theme-name probed for "HighContrast".
bool isHighContrast();

// Apply a theme mode to a native window handle (HWND on Windows). On
// non-Windows platforms this is a no-op — the OS handles dark-mode drawing.
void applyMode(void* nativeWindowHandle, Mode mode);

// Set an explicit DWM border color on a top-level window. Windows-only;
// requires Win11 22H2+ for DWMWA_BORDER_COLOR (silently no-ops on older
// Windows). Use Color{} to opt back to the system default.
void setWindowBorderColor(void* nativeWindowHandle, Color color);

const Colors& defaultPalette(Mode mode);

}  // namespace theme

#if defined(XPGUI_PLATFORM_WINDOWS)

namespace win32 {

using ThemeChangeToken = uint64_t;

// Register a callback fired when the system theme changes. The callback runs
// on whichever thread invokes notifySystemThemeChanged(). Returns a token
// that can be passed to offSystemThemeChanged() to unregister.
ThemeChangeToken onSystemThemeChanged(std::function<void(theme::Mode)> cb);
void offSystemThemeChanged(ThemeChangeToken token);

// Host code calls this from its WM_SETTINGCHANGE("ImmersiveColorSet") handler
// to fan-out the new mode to all registered callbacks. Future versions may
// detect changes internally via a hidden message window, making this no-op.
void notifySystemThemeChanged(theme::Mode newMode);

// RAII wrapper for the pair of HBRUSHes used by Win32 dark-mode dialogs.
// Returns HBRUSH-as-void* so callers can include this header without
// pulling in <windows.h>.
class ThemeBrushes {
public:
    explicit ThemeBrushes(const theme::Colors& colors);
    ~ThemeBrushes();

    ThemeBrushes(const ThemeBrushes&) = delete;
    ThemeBrushes& operator=(const ThemeBrushes&) = delete;

    void* bg() const;      // HBRUSH for window/dialog background
    void* editBg() const;  // HBRUSH for edit/listbox background

    void recreate(const theme::Colors& colors);

private:
    void* hBgBrush_ = nullptr;
    void* hEditBgBrush_ = nullptr;
};

}  // namespace win32

#endif  // XPGUI_PLATFORM_WINDOWS

}  // namespace xpgui
