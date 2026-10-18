# Kite standalone plugin build bootstrap.
# When this plugin is built from Kite's main tree, the parent project
# already provides Qt, Kite::SDK, output directories, and signing helpers.
# When this directory is configured as its own repository, provide those
# pieces here so the plugin can be built with only an installed Kite SDK.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.21)
    project(SystemTray LANGUAGES CXX)

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

# SystemTray — Kite plugin (.mmco)
#
# Adds a QSystemTrayIcon for Kite with a right-click menu (Show / Hide /
# Quit / quick-launch instances) and an optional "minimize to tray" close
# filter on the main window.
#
# All behaviour goes through the MMCO S19/S20 API; this plugin does NOT
# touch core widgets directly.

set(SYSTEMTRAY_SOURCES
    SystemTrayPlugin.cpp
)

add_library(SystemTray MODULE ${SYSTEMTRAY_SOURCES})

target_link_libraries(SystemTray PRIVATE
    Kite::SDK
)

set_target_properties(SystemTray PROPERTIES
    PREFIX ""
    SUFFIX ".mmco"
    LIBRARY_OUTPUT_DIRECTORY "${KITE_PLUGIN_STAGING_DIR}"
)

foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG}" CFG_UPPER)
    set_target_properties(SystemTray PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${KITE_PLUGIN_STAGING_DIR}"
    )
endforeach()

install(TARGETS SystemTray
    LIBRARY DESTINATION "${MMCO_MODULES_DEST_DIR}"
)

if(Kite_PLUGIN_SIGN_ALL)
    mmco_sign_plugin(SystemTray)
endif()
