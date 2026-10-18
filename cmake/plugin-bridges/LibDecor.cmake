# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# libdecor plugin bridge — exposes the bundled libdecor headers so the
# DisplayServerTweaks plugin can `dlopen()` libdecor at runtime without
# baking a hard link dependency into the .mmco.
#
# We deliberately do NOT build libdecor here:
#   1. libdecor wants Meson, not CMake.
#   2. Hard-linking libdecor would require Wayland at .mmco load time
#      even on X11-only systems, which would prevent the plugin from
#      loading on those systems.
#
# Instead, we:
#   • install or use the system's libdecor at runtime (the plugin
#     `dlopen()`s `libdecor-0.so.0`); and
#   • use the headers from the submodule at build time so we have a
#     stable, reproducible struct/enum layout regardless of what
#     version the host happens to install.
#
# Public targets:
#   KitePlugins::LibDecorHeaders — INTERFACE target carrying just the
#                                    include directory.

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
