#include "xpgui/platform.h"

#if defined(XPGUI_PLATFORM_LINUX)

#include "xpgui/theme.h"

#include <cstring>

#include <gtk/gtk.h>

namespace xpgui {
namespace theme {

Mode systemMode() {
    GtkSettings* s = gtk_settings_get_default();
    if (!s) return Mode::Light;  // GTK not initialised — host must call gtk_init() first

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

bool isHighContrast() {
    GtkSettings* s = gtk_settings_get_default();
    if (!s) return false;
    gchar* theme_name = nullptr;
    g_object_get(s, "gtk-theme-name", &theme_name, NULL);
    bool hc = false;
    if (theme_name) {
        hc = std::strstr(theme_name, "HighContrast") != nullptr;
        g_free(theme_name);
    }
    return hc;
}

void applyMode(void* /*nativeWindowHandle*/, Mode /*mode*/) {
    // GTK draws its own dark mode based on the user's GtkSettings.
}

const Colors& defaultPalette(Mode /*mode*/) {
    static const Colors c{};
    return c;
}

}  // namespace theme
}  // namespace xpgui

#endif  // XPGUI_PLATFORM_LINUX
