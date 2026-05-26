#pragma once

#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_WINDOWS)

#include "xpgui/theme.h"

namespace xpgui {

namespace win32 {

// Palette used to paint owner-drawn popup menus in dark mode. Set once by the
// host via setDarkMenuPalette() and re-set whenever the host's theme changes.
struct DarkMenuPalette {
    theme::Color background;     // popup background
    theme::Color text;           // item text
    theme::Color textDisabled;   // grayed item text
    theme::Color hotBackground;  // selected / hot item background
    theme::Color separator;      // separator line
};

// Replace the process-global palette used by handleDarkMenuDraw. Cheap to
// call; safe to invoke before applyDarkMenu.
void setDarkMenuPalette(const DarkMenuPalette& palette);

// Mark every item in `hMenu` as MFT_OWNERDRAW (preserving MFT_SEPARATOR) and
// stash a per-item context (label, accel substring, hasSubmenu, separator
// flag) so handleDarkMenuMeasure / handleDarkMenuDraw can repaint the item.
// Recurses into submenus when `recursive` is true. Idempotent: re-applying
// updates the stored context in place.
//
// `hMenu` is an HMENU passed as void* to keep <windows.h> out of this header
// (same convention used by theme::applyMode for HWND).
void applyDarkMenu(void* hMenu, bool recursive);

// Reverse of applyDarkMenu: restore items to plain MF_STRING with their
// original label text and free the per-item context. Safe to call on a menu
// that was never marked.
void restoreStandardMenu(void* hMenu, bool recursive);

// Forward from the host WndProc's WM_MEASUREITEM / WM_DRAWITEM cases when the
// struct's CtlType == ODT_MENU. Returns true when the message was handled;
// the host should then `return TRUE;` from its WndProc.
//
// Arguments are MEASUREITEMSTRUCT* / DRAWITEMSTRUCT* (as void*).
bool handleDarkMenuMeasure(void* measureItemStruct);
bool handleDarkMenuDraw(void* drawItemStruct);

}  // namespace win32

}  // namespace xpgui

#endif  // XPGUI_PLATFORM_WINDOWS
