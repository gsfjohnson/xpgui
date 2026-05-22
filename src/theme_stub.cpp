#include "xpgui/platform.h"

#if !defined(XPGUI_PLATFORM_WINDOWS)

#include "xpgui/theme.h"

namespace xpgui {
namespace theme {

// Default to Light on non-Windows hosts. macOS/Linux toolkits draw their
// own controls and don't need a palette query from xpgui yet.
Mode systemMode() {
    return Mode::Light;
}

void applyMode(void* /*nativeWindowHandle*/, Mode /*mode*/) {
    // Non-Windows hosts let the OS / toolkit handle dark-mode drawing.
}

const Colors& defaultPalette(Mode /*mode*/) {
    static const Colors c{};
    return c;
}

}  // namespace theme
}  // namespace xpgui

#endif  // !XPGUI_PLATFORM_WINDOWS
