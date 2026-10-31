# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# Exposes the bundled libdecor headers as `KitePlugins::LibDecorHeaders`. libdecor
# itself is not built (it needs Meson) and not linked: a hard link would need
# Wayland to load the .mmco on X11-only systems, so the plugin dlopen()s
# libdecor-0.so.0. The submodule headers keep the struct layout stable.

if(TARGET KitePlugins::LibDecorHeaders)
    return()
endif()

set(_libdecor_root "${CMAKE_SOURCE_DIR}/libraries/libdecor")

if(NOT EXISTS "${_libdecor_root}/src/libdecor.h")
    message(STATUS
        "libdecor submodule not initialised at libraries/libdecor. "
        "The DisplayServerTweaks plugin will be built without the "
        "libdecor module. "
        "Run: git submodule update --init libraries/libdecor")
    return()
endif()

add_library(KitePlugins::LibDecorHeaders INTERFACE IMPORTED GLOBAL)
target_include_directories(KitePlugins::LibDecorHeaders INTERFACE
    ${_libdecor_root}/src
)

message(STATUS
    "libdecor bridge configured: headers only "
    "(runtime resolution is dlopen-based)")
