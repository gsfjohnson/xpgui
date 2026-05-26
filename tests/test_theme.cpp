// Smoke test for xpgui theme API. Plain assertions, no test framework.
//
// On Windows: exercises systemMode() / defaultPalette() / ThemeBrushes /
// onSystemThemeChanged + notify path.
// On non-Windows: only the cross-platform subset is exercised.

#include "xpgui/platform.h"
#include "xpgui/theme.h"

#include <cassert>
#include <cstdio>

int main() {
    using namespace xpgui;

    // Color round-trip
    {
        constexpr auto c = theme::Color::fromCOLORREF(0x00112233);
        static_assert(c.r == 0x33, "r byte");
        static_assert(c.g == 0x22, "g byte");
        static_assert(c.b == 0x11, "b byte");
        static_assert(c.toCOLORREF() == 0x00112233, "roundtrip");
    }

    // systemMode() returns something — value depends on host settings.
    auto mode = theme::systemMode();
    (void)mode;

    // isHighContrast() returns without crashing. Value depends on host
    // accessibility settings; we only assert the call is safe.
    bool hc = theme::isHighContrast();
    (void)hc;

    // defaultPalette returns stable references.
    const auto& dark = theme::defaultPalette(theme::Mode::Dark);
    const auto& light = theme::defaultPalette(theme::Mode::Light);
    assert(&dark != &light);

#if defined(XPGUI_PLATFORM_WINDOWS)
    // Dark text should be brighter than light text — sanity check the
    // palette wiring without depending on exact pixel values.
    assert(dark.text.r > light.text.r || dark.bg.r < light.bg.r);

    // ThemeBrushes lifecycle.
    {
        win32::ThemeBrushes brushes(dark);
        assert(brushes.bg() != nullptr);
        assert(brushes.editBg() != nullptr);
        brushes.recreate(light);
        assert(brushes.bg() != nullptr);
        assert(brushes.editBg() != nullptr);
    }

    // Callback registry + notify fan-out.
    int hits = 0;
    theme::Mode lastSeen = theme::Mode::Light;
    auto tok = win32::onSystemThemeChanged([&](theme::Mode m) {
        ++hits;
        lastSeen = m;
    });
    assert(tok != 0);

    win32::notifySystemThemeChanged(theme::Mode::Dark);
    assert(hits == 1);
    assert(lastSeen == theme::Mode::Dark);

    win32::notifySystemThemeChanged(theme::Mode::Light);
    assert(hits == 2);
    assert(lastSeen == theme::Mode::Light);

    win32::offSystemThemeChanged(tok);
    win32::notifySystemThemeChanged(theme::Mode::Dark);
    assert(hits == 2);  // unchanged after unregister

    // Null hwnd is safe.
    theme::applyMode(nullptr, theme::Mode::Dark);

    // Empty callback is rejected.
    auto zero = win32::onSystemThemeChanged({});
    assert(zero == 0);
#else
    // Non-Windows: detection returns whichever mode the host system reports.
    // Assert only that it returns a valid enumerator and that applyMode is safe.
    (void)dark;
    (void)light;
    auto m = theme::systemMode();
    assert(m == theme::Mode::Light || m == theme::Mode::Dark);
    std::printf("xpgui_test_theme: systemMode=%s\n",
                m == theme::Mode::Dark ? "dark" : "light");
    theme::applyMode(nullptr, theme::Mode::Dark);
#endif

    std::puts("xpgui_test_theme: ok");
    return 0;
}
