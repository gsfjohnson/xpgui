#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_MACOS)

#include "xpgui/theme.h"

#import <AppKit/AppKit.h>

namespace xpgui {
namespace theme {

Mode systemMode() {
    NSAppearance* appearance = nil;
    if (NSApp) {
        if (@available(macOS 10.14, *)) {
            appearance = [NSApp effectiveAppearance];
        }
    }
    if (!appearance) {
        if (@available(macOS 10.14, *)) {
            appearance = [NSAppearance currentAppearance];
        }
    }
    if (!appearance) return Mode::Light;

    if (@available(macOS 10.14, *)) {
        NSString* best = [appearance bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua,
            NSAppearanceNameDarkAqua,
        ]];
        return [best isEqualToString:NSAppearanceNameDarkAqua] ? Mode::Dark : Mode::Light;
    }
    return Mode::Light;
}

bool isHighContrast() {
    @autoreleasepool {
        return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
    }
}

void applyMode(void* /*nativeWindowHandle*/, Mode /*mode*/) {
    // Cocoa controls draw their own dark mode based on NSApp.appearance.
    // Hosts that want to override per-window can set NSWindow.appearance directly.
}

const Colors& defaultPalette(Mode mode) {
    // Cocoa draws its own theming, so these values are unused placeholders — but
    // keep a distinct, stable instance per mode to honor the per-mode reference
    // contract (defaultPalette(Dark) and (Light) must not alias).
    static const Colors dark{};
    static const Colors light{};
    return mode == Mode::Dark ? dark : light;
}

}  // namespace theme
}  // namespace xpgui

#endif  // XPGUI_PLATFORM_MACOS
