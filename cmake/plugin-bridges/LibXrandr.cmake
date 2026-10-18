# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# libXrandr plugin bridge — exposes bundled libXrandr headers for the
# DisplayServerTweaks plugin. Like libdecor, we resolve libXrandr.so via
# dlopen() at runtime so the plugin remains loadable on Wayland-only
# systems and inside Flatpak sandboxes that don't expose the X libs.
#
# Public targets:
#   KitePlugins::LibXrandrHeaders — INTERFACE with the include dir.

if(TARGET KitePlugins::LibXrandrHeaders)
    return()
endif()

set(_libXrandr_root "${CMAKE_SOURCE_DIR}/libraries/libXrandr")

if(NOT EXISTS "${_libXrandr_root}/include/X11/extensions/Xrandr.h")
    message(STATUS
        "libXrandr submodule not initialised at libraries/libXrandr. "
        "The DisplayServerTweaks plugin will be built without the "
        "xrandr module. "
        "Run: git submodule update --init libraries/libXrandr")
    return()
endif()

add_library(KitePlugins::LibXrandrHeaders INTERFACE IMPORTED GLOBAL)
target_include_directories(KitePlugins::LibXrandrHeaders INTERFACE
    ${_libXrandr_root}/include
)

message(STATUS
    "libXrandr bridge configured: headers only "
    "(runtime resolution is dlopen-based)")
