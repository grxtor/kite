# Standalone build bootstrap: when this directory is its own project, provide what the Kite tree
# normally gives a plugin (Qt, Kite::SDK, output directories, signing helpers).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(ErrorOracle LANGUAGES CXX)

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    set(Kite_QT_VERSION_MAJOR "6" CACHE STRING "Major Qt version to build against (5 or 6)")
    set(QT_VERSION_MAJOR "${Kite_QT_VERSION_MAJOR}")

    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Network)
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
# ErrorOracle: Kite plugin (.mmco). LLM-free crash / log analyser: a built-in rule pack
# (rules/*.json, loaded once at init) mapping regex patterns to advice, plus a learning layer that
# tracks (rule, instance) -> outcome pairs under <plugin_data>/learn.json and re-ranks suggestions
# per instance (local statistics, no network). Extra rule packs go in
# <plugin_data>/userrules/*.json without recompiling; the "Reload rules" button reloads them.

set(ERR_SOURCES
    ErrorOraclePlugin.cpp
    RuleEngine.h
    RuleEngine.cpp
    LogIngester.h
    LogIngester.cpp
    LearningStore.h
    LearningStore.cpp
    AnalysisPage.h
    AnalysisPage.cpp
)

add_library(ErrorOracle MODULE ${ERR_SOURCES})

target_link_libraries(ErrorOracle PRIVATE
    Kite::SDK
)

target_include_directories(ErrorOracle PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(ErrorOracle PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${KITE_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(ErrorOracle PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${KITE_PLUGIN_STAGING_DIR}"
    )
endforeach()

# Install the bundled rule pack as PLUGIN DATA, not next to the .mmco: on macOS codesign treats
# Contents/PlugIns/mmcmodules as nested code and rejects JSON files there ("code object is not
# signed at all"). MMCO_PLUGIN_DATA_DEST_DIR is <install>/mmcmodules on Linux/Windows (next to the
# .mmco) and Kite.app/Contents/Resources/MMCOPluginData on macOS.
# ErrorOraclePlugin::builtInRulesDir probes the platform location first, then the legacy
# next-to-.mmco path.
install(DIRECTORY rules
    DESTINATION "${MMCO_PLUGIN_DATA_DEST_DIR}/ErrorOracle"
    FILES_MATCHING PATTERN "*.json"
)

# Stage rule files next to the .mmco in the build tree.
file(GLOB _eo_rule_files "${CMAKE_CURRENT_SOURCE_DIR}/rules/*.json")
foreach(_rule ${_eo_rule_files})
    file(COPY "${_rule}"
         DESTINATION "${KITE_PLUGIN_STAGING_DIR}/ErrorOracle/rules")
endforeach()

install(TARGETS ErrorOracle
    LIBRARY DESTINATION "${MMCO_MODULES_DEST_DIR}"
)

if(Kite_PLUGIN_SIGN_ALL)
    mmco_sign_plugin(ErrorOracle)
endif()
