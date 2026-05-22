# xpgui

Cross-platform C++17 GUI scaffolding for native desktop apps on Windows, macOS, and Linux.

xpgui owns the boring, fiddly, per-platform code that every native desktop app re-discovers — Win32 dark mode, tray/menu-bar status items, native file pickers, run-at-startup — exposed behind a unified API. It is **not** a widget toolkit or Qt/wxWidgets replacement; host projects keep their own dialogs, view models, and main-loop integration.

> ⚠️ **Private / pre-1.0.** xpgui is in flux while its first consumers (HomeDrive, Basu, ...) shake out the API. Breaking changes are expected. Consumers pin a commit SHA via git submodule. Public 1.0 ships only after the validation gate described in `DESIGN.md` is met.

## Building

xpgui is intended to be consumed as a git submodule:

```bash
git submodule add git@github.com:gsfjohnson/xpgui.git third_party/xpgui
```

In the consumer's `CMakeLists.txt`:

```cmake
add_subdirectory(third_party/xpgui)
target_link_libraries(my_app PRIVATE xpgui::xpgui)
```

To build standalone (mostly for development/CI of xpgui itself):

```bash
cmake -B build -DXPGUI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Design

See [`DESIGN.md`](DESIGN.md) for the full design rationale, phased roadmap, platform matrix, and out-of-scope items.
