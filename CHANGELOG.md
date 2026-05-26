# xpgui changelog

Pre-1.0 — no semver, breaking changes expected. Consumers pin a commit SHA.

## Unreleased

### Added
- Windows owner-drawn dark-mode popup menus: `xpgui::win32::applyDarkMenu`, `restoreStandardMenu`, `handleDarkMenuMeasure`, `handleDarkMenuDraw`, and `DarkMenuPalette`. Header at `<xpgui/menu.h>`. Host marks every menu item `MFT_OWNERDRAW` via `applyDarkMenu` and forwards `WM_MEASUREITEM` / `WM_DRAWITEM` cases (`CtlType == ODT_MENU`) to the handlers; xpgui paints background / text / checkmark / submenu arrow / accelerator using the supplied palette. Idempotent and reversible. Implemented in `src/menu_win32.cpp`.
- Phase 0: repo scaffolding. `xpgui::xpgui` static library target, `XPGUI_PLATFORM_*` macros in `<xpgui/platform.h>`, callback-based logging API (`xpgui::setLogCallback`, `xpgui::Level`, internal `xpgui::log::{debug,info,warn,error}`), smoke test (`tests/test_log.cpp`).
- Phase 1: cross-platform `xpgui::theme` API — `Mode`, `Color` (with `fromCOLORREF`/`toCOLORREF`), `Colors`, `systemMode()`, `systemUsesDarkMode()`, `applyMode()`, `defaultPalette()`.
  - Windows: full dark-mode plumbing in `theme_win32.cpp` — titlebar via `DwmSetWindowAttribute`, child controls via undocumented uxtheme ordinals, default light/dark palettes, `xpgui::win32::ThemeBrushes` RAII wrapper, theme-change callback registry (`onSystemThemeChanged` / `offSystemThemeChanged` / `notifySystemThemeChanged`).
  - macOS: `theme_macos.mm` queries `NSAppearance.effectiveAppearance` via `bestMatchFromAppearancesWithNames:`. `applyMode` is a no-op (Cocoa handles dark drawing).
  - Linux: `theme_linux_gtk3.cpp` and `theme_linux_gtk4.cpp` query `GtkSettings` (`gtk-application-prefer-dark-theme` then `gtk-theme-name` substring). GTK major version selected via `XPGUI_GTK_VERSION` CMake cache var (default 3) — must match the host process's GTK link.
  - Smoke test `tests/test_theme.cpp` covers the cross-platform API on all platforms and the Windows-only callback fan-out + `ThemeBrushes` lifecycle.
