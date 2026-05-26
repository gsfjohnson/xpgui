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

const Colors& defaultPalette(Mode /*mode*/) {
    static const Colors c{};
    return c;
}

}  // namespace theme
}  // namespace xpgui

#endif  // XPGUI_PLATFORM_MACOS
