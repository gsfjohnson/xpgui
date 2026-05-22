# xpgui changelog

Pre-1.0 — no semver, breaking changes expected. Consumers pin a commit SHA.

## Unreleased

### Added
- Phase 0: repo scaffolding. `xpgui::xpgui` static library target, `XPGUI_PLATFORM_*` macros in `<xpgui/platform.h>`, callback-based logging API (`xpgui::setLogCallback`, `xpgui::Level`, internal `xpgui::log::{debug,info,warn,error}`), smoke test (`tests/test_log.cpp`).
