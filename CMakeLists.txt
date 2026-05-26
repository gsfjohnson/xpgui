cmake_minimum_required(VERSION 3.16)
project(xpgui LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XPGUI_BUILD_TESTS "Build xpgui smoke tests" OFF)
option(XPGUI_BUILD_EXAMPLES "Build xpgui example apps" OFF)

# Platform detection — independent of any host-project defines (HD_PLATFORM_*).
# MSYS2 Clang targets Windows but CMake sees it as UNIX, hence MSYSTEM check.
if(WIN32 OR DEFINED ENV{MSYSTEM})
    set(XPGUI_PLATFORM windows)
elseif(APPLE)
    set(XPGUI_PLATFORM macos)
elseif(UNIX)
    set(XPGUI_PLATFORM linux)
else()
    message(FATAL_ERROR "xpgui: unsupported platform")
endif()
message(STATUS "xpgui: platform=${XPGUI_PLATFORM}")

set(XPGUI_PUBLIC_HEADERS
    include/xpgui/log.h
    include/xpgui/platform.h
    include/xpgui/theme.h
)

set(XPGUI_SOURCES
    src/log.cpp
)

# Platform source selection.
if(XPGUI_PLATFORM STREQUAL "windows")
    list(APPEND XPGUI_SOURCES src/theme_win32.cpp)
elseif(XPGUI_PLATFORM STREQUAL "macos")
    enable_language(OBJCXX)
    list(APPEND XPGUI_SOURCES src/theme_macos.mm)
elseif(XPGUI_PLATFORM STREQUAL "linux")
    # GTK major version must match the host process. GTK3 and GTK4 cannot
    # coexist in one process. Default to GTK3 (matches current consumers and
    # the DESIGN.md commitment); set XPGUI_GTK_VERSION=4 to opt in.
    set(XPGUI_GTK_VERSION "3" CACHE STRING
        "GTK major version to link (3 or 4). Must match the host process.")
    set_property(CACHE XPGUI_GTK_VERSION PROPERTY STRINGS "3" "4")
    find_package(PkgConfig REQUIRED)
    if(XPGUI_GTK_VERSION STREQUAL "4")
        pkg_check_modules(XPGUI_GTK REQUIRED gtk4)
        list(APPEND XPGUI_SOURCES src/theme_linux_gtk4.cpp)
    else()
        pkg_check_modules(XPGUI_GTK REQUIRED gtk+-3.0)
        list(APPEND XPGUI_SOURCES src/theme_linux_gtk3.cpp)
    endif()
    message(STATUS "xpgui: linux GTK=${XPGUI_GTK_VERSION}")
endif()

add_library(xpgui STATIC ${XPGUI_SOURCES} ${XPGUI_PUBLIC_HEADERS})
add_library(xpgui::xpgui ALIAS xpgui)

target_include_directories(xpgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(xpgui PUBLIC cxx_std_17)

if(XPGUI_PLATFORM STREQUAL "windows")
    target_link_libraries(xpgui PUBLIC dwmapi uxtheme gdi32 user32)
elseif(XPGUI_PLATFORM STREQUAL "macos")
    target_link_libraries(xpgui PUBLIC "-framework AppKit" "-framework Foundation")
elseif(XPGUI_PLATFORM STREQUAL "linux")
    target_include_directories(xpgui PRIVATE ${XPGUI_GTK_INCLUDE_DIRS})
    target_link_libraries(xpgui PUBLIC ${XPGUI_GTK_LIBRARIES})
    target_compile_options(xpgui PRIVATE ${XPGUI_GTK_CFLAGS_OTHER})
endif()

if(XPGUI_BUILD_TESTS)
    enable_testing()
    add_executable(xpgui_test_log tests/test_log.cpp)
    target_link_libraries(xpgui_test_log PRIVATE xpgui)
    add_test(NAME xpgui_test_log COMMAND xpgui_test_log)

    add_executable(xpgui_test_theme tests/test_theme.cpp)
    target_link_libraries(xpgui_test_theme PRIVATE xpgui)
    add_test(NAME xpgui_test_theme COMMAND xpgui_test_theme)
endif()
