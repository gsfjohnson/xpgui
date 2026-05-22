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
)

set(XPGUI_SOURCES
    src/log.cpp
)

add_library(xpgui STATIC ${XPGUI_SOURCES} ${XPGUI_PUBLIC_HEADERS})
add_library(xpgui::xpgui ALIAS xpgui)

target_include_directories(xpgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(xpgui PUBLIC cxx_std_17)

if(XPGUI_BUILD_TESTS)
    enable_testing()
    add_executable(xpgui_test_log tests/test_log.cpp)
    target_link_libraries(xpgui_test_log PRIVATE xpgui)
    add_test(NAME xpgui_test_log COMMAND xpgui_test_log)
endif()
