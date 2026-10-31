# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Exposes the bundled libXrandr headers as `KitePlugins::LibXrandrHeaders`. Like
# libdecor, libXrandr.so is dlopen()ed so the plugin loads on Wayland-only
# systems and in Flatpak sandboxes without the X libs.

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
