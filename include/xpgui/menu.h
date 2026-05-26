#pragma once

#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_WINDOWS)

#include "xpgui/theme.h"

namespace xpgui {

namespace win32 {

// Palette used to paint owner-drawn popup menus. The host pushes whichever
// palette matches the current app theme (light or dark) via setMenuPalette().
// The struct name keeps the historical "Dark" prefix for ABI continuity even
// though it now carries both light and dark palettes depending on app theme.
struct DarkMenuPalette {
    theme::Color background;     // popup background
    theme::Color text;           // item text
    theme::Color textDisabled;   // grayed item text
    theme::Color hotBackground;  // selected / hot item background
    theme::Color separator;      // separator line
};

// Replace the process-global palette used by handleDarkMenuDraw. Cheap to
// call; safe to invoke before applyOwnerDrawMenu.
void setDarkMenuPalette(const DarkMenuPalette& palette);

// Mark every item in `hMenu` as MFT_OWNERDRAW (preserving MFT_SEPARATOR) and
// stash a per-item context (label, accel substring, hasSubmenu, separator
// flag) so handleDarkMenuMeasure / handleDarkMenuDraw can repaint the item
// using whatever palette is currently set. Works for both light and dark
// modes — caller is responsible for pushing the right palette first.
// Recurses into submenus when `recursive` is true. Idempotent: re-applying
// updates the stored context in place.
//
// `hMenu` is an HMENU passed as void* to keep <windows.h> out of this header
// (same convention used by theme::applyMode for HWND).
void applyOwnerDrawMenu(void* hMenu, bool recursive);

// Reverse of applyOwnerDrawMenu: restore items to plain MF_STRING with their
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

// RAII helper to apply DWMWA_USE_IMMERSIVE_DARK_MODE to popup menu windows
// (class #32768) created while the scope is alive on the current thread. Wrap
// TrackPopupMenu calls with this so the popup's non-client frame matches the
// app window's DWM frame in color and thickness.
//
// Implementation installs a thread-local WH_CBT hook; construction/destruction
// are cheap. Re-entrant on the same thread (nested scopes share the hook).
struct PopupFrameScope {
    explicit PopupFrameScope(theme::Mode mode);
    ~PopupFrameScope();
    PopupFrameScope(const PopupFrameScope&) = delete;
    PopupFrameScope& operator=(const PopupFrameScope&) = delete;
};

}  // namespace win32

}  // namespace xpgui

#endif  // XPGUI_PLATFORM_WINDOWS
