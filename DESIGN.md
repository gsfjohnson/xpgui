# xpgui — Design

Cross-platform C++ GUI scaffolding toolkit. This document captures the design rationale, scope, validation criteria, and roadmap. Phase 0/1 extraction tracking specific to consumer projects lives in the consumer repos (e.g. HomeDrive's `pm/ABSTRACT_XP_GUI.md`).

---

## 1. Vision

`xpgui` is a small C++17 toolkit that provides the *scaffolding* a native desktop GUI app needs on Windows, macOS, and Linux — the parts that take hours to get right per-platform and look identical from app to app. It is **not** a widget toolkit, not a declarative UI framework, and not a Qt/wxWidgets replacement.

Concretely, xpgui owns the boring, fiddly, per-platform code that every native desktop app re-discovers:

- Win32 dark mode (titlebar + child controls + theme palette + brush lifecycle)
- (Future) Tray/menu-bar status item with menu, tooltip, notification
- (Future) Native folder/file picker
- (Future) Run-at-startup registration

Host projects keep their own dialogs, view models, business logic, and main-loop integration.

## 2. Validation Gate (when is xpgui "done")

xpgui is **private and in flux** until it has been used in production by the following projects:

1. **HomeDrive** — file sync, S3 backend (first consumer)
2. **Basu** — cross-platform notes app (next consumer)
3. Calendar app (planned)
4. Contacts app (planned)
5. Inventory management app (planned)

### v1.0 definition

xpgui ships v1.0 (drop "private", consider permissive license, accept external use) when **all** of:

- Used in production by **at least 3** of the apps above.
- **No public-API breaking change** in the prior 60 days.
- README + at least one minimal example app (~100 LOC) demonstrating each subsystem.
- CI green on the supported platform/compiler matrix (Section 5).
- A consumer project's `dark mode + tray + pickers + autostart` integration is **<200 LOC of glue**.

Until then, breaking changes are expected. Each consumer pins a commit SHA via submodule.

## 3. Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Namespace | `xpgui::` everything, with unified sub-namespaces (`xpgui::theme::`, `xpgui::tray::`) | Consumer code stays portable by default; call once, no-op on platforms without the feature |
| Platform-specific escape hatch | `xpgui::win32::`, `xpgui::macos::`, `xpgui::linux::` for inherently platform-only APIs (e.g. Win32 registry helpers) | Honest naming when there is no cross-platform analog |
| Object model | Hybrid: free functions for stateless utilities (e.g. `theme::systemUsesDarkMode()`), classes for resources with lifetime (e.g. `ThemeBrushes`, future `TrayIcon`) | Matches OS APIs where appropriate, RAII where useful |
| C++ standard | C++17 | Maximises adoption; HomeDrive (C++20) consumes it fine |
| Build kind | Always-compiled static library, single CMake target `xpgui::xpgui` | Simpler than per-platform split targets; consumers don't need to know which platform they're on at the CMake level |
| Lifecycle abstraction (`IAppUI`) | **Excluded from v0.1** | Keep xpgui as a toolkit, not a framework. Revisit after Basu reveals the actual common shape |
| Logging | Callback-based: `xpgui::setLogCallback([](Level, std::string_view){...})` | Zero public deps on spdlog or any logger; consumers wrap their own |
| Hashvatar | **Excluded** — its own micro-library | Not GUI scaffolding |
| Declarative form/dialog builder | **Excluded indefinitely** | Where most cross-platform GUI projects die; host view models and dialogs stay in host projects |

### Platform/compiler matrix

| Platform | Compiler | Minimum | Notes |
|---|---|---|---|
| Windows 11 | MSYS2 LLVM Clang | — | No MSVC, no GCC |
| macOS | Apple Clang | macOS 12 (Monterey) | OBJCXX (`.mm`) for Cocoa |
| Linux | Clang | GTK3 era (Ubuntu 22.04+) | GTK3 + libayatana-appindicator. GTK4 supported for `theme::systemMode()` detection (via `XPGUI_GTK_VERSION=4`); full GTK4 widget integration (tray etc.) deferred to v2.x. GTK3 and GTK4 cannot coexist in one process — xpgui's GTK version must match the host. |

### Repository layout

```
xpgui/
  CMakeLists.txt
  README.md
  DESIGN.md                   (this document)
  LICENSE                     (private placeholder; firmed up at v1.0)
  include/xpgui/
    theme.h                   # Cross-platform theme/dark-mode API (Phase 1)
    log.h                     # Log level enum + setLogCallback
    platform.h                # XPGUI_PLATFORM_WINDOWS / _MACOS / _LINUX
    win32/registry.h          # Win32-only escape hatch (later phases)
  src/
    log.cpp
    theme_win32.cpp           # Dark mode, titlebar, brushes, ctlcolor (Phase 1)
    theme_macos.mm            # NSAppearance hooks, mostly no-op (Phase 1)
    theme_linux_gtk3.cpp      # GTK3 GtkSettings dark-mode detection (Phase 1)
    theme_linux_gtk4.cpp      # GTK4 GtkSettings dark-mode detection (Phase 1)
  examples/
    minimal_dark/             # Single-window app demonstrating dark mode (Phase 1)
  tests/
    test_log.cpp              # Callback wiring (Phase 0)
    test_theme.cpp            # What can be tested headlessly (Phase 1)
```

## 4. Roadmap (Phase 3+)

Phase 0 (repo scaffolding) and Phase 1 (Win32 dark mode extraction) are tracked in each consumer's own plan document. The phases below are sketched but not committed — each lands only when a consumer project actually needs it. Basu, then the calendar/contacts/inventory apps will drive ordering.

| Phase | Subsystem | Estimated scope |
|---|---|---|
| 3 | Tray / menu-bar status item (cross-platform `xpgui::tray::TrayIcon` with menu, tooltip, balloon notification) | Largest single subsystem — Win32 `Shell_NotifyIconW` + GTK3 AppIndicator + Cocoa `NSStatusItem`. ~800 LOC across platforms. |
| 4 | Folder/file picker (`xpgui::pickFolder`, `xpgui::pickFile`) | `IFileDialog` / `GtkFileChooser` / `NSOpenPanel`. ~200 LOC. |
| 5 | Run-at-startup (`xpgui::autostart::isEnabled` / `setEnabled`) | Win32 registry / Linux `.desktop` / macOS LaunchAgent — quite different per platform. ~250 LOC. |
| 6 | Optional `xpgui::App` lifecycle base class | Only if a clear shared shape emerges across HomeDrive, Basu, and the calendar app. |
| 7 | "About this application" window | Possible exception to the "host project owns dialogs" rule. Useful only if it can be done without baking host-app branding into xpgui. |

## 5. Out of Scope (Permanent)

- **Host-app view models** (e.g. HomeDrive's `FsConfigViewModel`, AWS regions, S3 auth modes, conflict view models). Stay in the host project.
- **Host-app dialogs** — config, conflict, status, transfer-progress dialogs stay in each host project. Possible narrow exception: a generic "About" window in a future phase.
- **Declarative form/dialog builder** — translating field descriptions into native widgets. This is where most cross-platform GUI projects die; xpgui does not attempt it.
- **Hashvatar** — pure compute, will be its own micro-library.
- **Widget toolkit** — xpgui is not a Qt/wxWidgets/GTKmm replacement. Host projects keep using native widgets directly.

## 6. Build & Distribution Rules

- **No host-project-specific defines** (`HD_PLATFORM_*`, `HD_DEV_BUILD`, etc.) — xpgui detects its own platform.
- **No public spdlog dependency.** xpgui's headers must not transitively `#include <spdlog/...>`. Internally a thin log wrapper forwards to the consumer's callback.
- **No public AWS / nlohmann / inih dependency.** xpgui knows nothing about S3, JSON, or INI.
- **Single CMake target** (`xpgui::xpgui`). Platform sources are conditionally compiled inside the target, not split into separate targets.
- **Submodule convention**: consumers `git submodule add <url> third_party/xpgui` and `add_subdirectory(third_party/xpgui)`. Each consumer pins a commit SHA.
- **Versioning**: pre-1.0, no semver. Changes tracked in `CHANGELOG.md` with date + brief description. Bump to 1.0.0 only when the validation gate (Section 2) is met.

## 7. Resolved Decisions

- **Color type**: single `xpgui::theme::Color { uint8_t r, g, b }` struct with `fromCOLORREF`/`toCOLORREF` conversion methods. One source of truth, no `windows.h` in public headers, trivial 1-line conversion at Win32 call sites, extensible to alpha later without API doubling.
- **Repo**: `github.com/gsfjohnson/xpgui` (private). Submodule URL: `git@github.com:gsfjohnson/xpgui.git`. Mounted at `third_party/xpgui/` in each consumer.
- **Apply API**: `xpgui::theme::applyMode(handle, mode)` always takes an explicit `Mode`. The host project decides whether to mirror the system or honour a user override; xpgui never silently reads system state inside an `apply*` call.
