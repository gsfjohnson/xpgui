#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_LINUX)

#include "xpgui/theme.h"

#include <cstring>

#include <gtk/gtk.h>

namespace xpgui {
namespace theme {

// GTK4 detection. Same approach as the GTK3 variant: GtkSettings still exposes
// gtk-application-prefer-dark-theme (deprecated since 4.10 but functional) and
// gtk-theme-name. A future variant could depend on libadwaita's AdwStyleManager
// for richer support; keeping that out of xpgui core to avoid pulling adwaita
// onto every Linux consumer.
Mode systemMode() {
    GtkSettings* s = gtk_settings_get_default();
    if (!s) return Mode::Light;  // GTK not initialised

    gboolean prefer_dark = FALSE;
    g_object_get(s, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    if (prefer_dark) return Mode::Dark;

    gchar* theme_name = nullptr;
    g_object_get(s, "gtk-theme-name", &theme_name, NULL);
    bool dark = false;
    if (theme_name) {
        for (gchar* p = theme_name; *p; ++p) *p = g_ascii_tolower(*p);
        dark = std::strstr(theme_name, "dark") != nullptr;
        g_free(theme_name);
    }
    return dark ? Mode::Dark : Mode::Light;
}

void applyMode(void* /*nativeWindowHandle*/, Mode /*mode*/) {
    // GTK4 draws its own dark mode based on the user's GtkSettings.
}

const Colors& defaultPalette(Mode /*mode*/) {
    static const Colors c{};
    return c;
}

}  // namespace theme
}  // namespace xpgui

#endif  // XPGUI_PLATFORM_LINUX
