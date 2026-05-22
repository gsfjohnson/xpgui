#pragma once

// xpgui platform detection. Defined independently of any host-project macros
// (e.g. HomeDrive's HD_PLATFORM_*), so xpgui can be vendored into any project.

#if defined(_WIN32)
#  define XPGUI_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#  define XPGUI_PLATFORM_MACOS 1
#elif defined(__linux__)
#  define XPGUI_PLATFORM_LINUX 1
#else
#  error "xpgui: unsupported platform"
#endif
