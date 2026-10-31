# In-tree plugin registry. Each subdirectory builds a .mmco, placed in ${KITE_PLUGIN_STAGING_DIR}
# so the launcher finds it in the app-relative mmcmodules/ directory.

set(KITE_PLUGIN_STAGING_DIR "${CMAKE_BINARY_DIR}/mmcmodules" CACHE PATH
    "Directory where built .mmco plugins are placed for runtime discovery")

file(MAKE_DIRECTORY "${KITE_PLUGIN_STAGING_DIR}")

# Signing helper: per-plugin CMakeLists.txt opt in to mmco_sign_plugin(); Kite_PLUGIN_SIGN_ALL
# signs every plugin.
include(MMCOSign)

# Staging plugins
if(Kite_STAGING_PLUGINS)
    add_subdirectory(staging)
endif()

# Register plugins here

# KITE_IN_TREE_PLUGIN_TARGETS lists every in-tree plugin target. Staging plugins (plugins/staging/,
# behind the off-by-default Kite_STAGING_PLUGINS option) are not in it. A plugin that moves into
# the launcher is dropped here and added to launcher/plugin/CoreSupersededPlugins.h, so stale .mmco
# files are refused instead of doubling up on the core feature. Already absorbed: Filelink,
# PackPortal, SkinManager, BackupSystem, NewsViewer.
set(KITE_IN_TREE_PLUGIN_TARGETS
    SystemTray
    DesktopNotifier
    DiscordRPC
    GitVersioning
    OfflineWiki
)

add_subdirectory(SystemTray)
add_subdirectory(DesktopNotifier)
add_subdirectory(DiscordRPC)
add_subdirectory(GitVersioning)
add_subdirectory(OfflineWiki)

if(UNIX AND NOT APPLE)
    add_subdirectory(NVIDIAPrime)
    add_subdirectory(LinuxPerf)
    list(APPEND KITE_IN_TREE_PLUGIN_TARGETS NVIDIAPrime LinuxPerf)
endif()

# Signing is wired inside each plugin's own CMakeLists.txt: CMake 3.27+ requires
# add_custom_command(TARGET ... POST_BUILD) to come from the CMakeLists.txt that created the
# target. KITE_IN_TREE_PLUGIN_TARGETS does not drive signing.

# Plugin runtime dependencies (Windows). Neither deployment path covers plugins: the launcher's
# install(RUNTIME_DEPENDENCY_SET) only resolves targets that joined that set, and windeployqt runs
# on kite.exe alone. A Qt module only a plugin links would never reach the package and the plugin
# would die at startup with LoadLibrary error 126. $<TARGET_RUNTIME_DLLS:tgt> lists the shared
# libraries a target links, transitively, for every registered plugin. The build tree gets no copy:
# its Qt DLLs resolve through PATH for the launcher as well.
if(WIN32)
    set(KITE_PLUGIN_RUNTIME_DLLS "")
    foreach(_plugin_target IN LISTS KITE_IN_TREE_PLUGIN_TARGETS)
        list(APPEND KITE_PLUGIN_RUNTIME_DLLS
            "$<TARGET_RUNTIME_DLLS:${_plugin_target}>")
    endforeach()
    unset(_plugin_target)

    if(KITE_PLUGIN_RUNTIME_DLLS)
        # Mostly Qt libraries the launcher already installs (re-installing an identical file is a
        # no-op); the point is the rest.
        install(FILES ${KITE_PLUGIN_RUNTIME_DLLS}
            DESTINATION "${BINARY_DEST_DIR}")
    endif()
endif()
