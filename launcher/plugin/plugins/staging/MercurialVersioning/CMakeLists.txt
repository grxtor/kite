# Kite standalone plugin build bootstrap.
# When built from Kite's main tree, the parent project already provides
# Kite::SDK_C, output directories, and signing helpers. When configured
# as its own repository, provide those pieces here so the plugin can be
# built with only an installed Kite SDK — and crucially WITHOUT Qt,
# since this is a plain-C plugin.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(MercurialVersioning LANGUAGES C)

    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)

    find_package(Kite_SDK REQUIRED)

    set(KITE_PLUGIN_STAGING_DIR "${CMAKE_BINARY_DIR}/mmcmodules" CACHE PATH
        "Directory where built .mmco plugins are placed")
    set(MMCO_MODULES_DEST_DIR "lib/mmcmodules" CACHE PATH
        "Install directory for .mmco plugins")
    set(MMCO_PLUGIN_DATA_DEST_DIR "share/kite/mmcmodules" CACHE PATH
        "Install directory for plugin data files")
endif()

# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0
#
# MercurialVersioning — Kite plugin (.mmco), written in plain C.
# Snapshots instance content as Mercurial commits via the S31
# process_run() host API. No Qt, no C++.

add_library(MercurialVersioning MODULE
    MercurialVersioningPlugin.c
)

# C ABI only — deliberately NOT linked against Qt or Kite::SDK (C++).
target_link_libraries(MercurialVersioning PRIVATE
    Kite::SDK_C
)

# Hide every symbol except the three the loader resolves
# (mmco_module_info / mmco_init / mmco_unload), which carry the
# MMCO_EXPORT visibility attribute.
set_target_properties(MercurialVersioning PROPERTIES
    C_VISIBILITY_PRESET hidden
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${KITE_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(MercurialVersioning PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${KITE_PLUGIN_STAGING_DIR}"
    )
endforeach()

install(TARGETS MercurialVersioning
    LIBRARY DESTINATION "${MMCO_MODULES_DEST_DIR}"
)

if(Kite_PLUGIN_SIGN_ALL)
    mmco_sign_plugin(MercurialVersioning)
endif()
