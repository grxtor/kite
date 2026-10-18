# Kite In-Tree Plugin Registry
#
# Each subdirectory here builds a .mmco shared module.
# Built modules are placed in ${KITE_PLUGIN_STAGING_DIR} so the
# launcher can discover them at runtime from the app-relative
# mmcmodules/ directory.

set(KITE_PLUGIN_STAGING_DIR "${CMAKE_BINARY_DIR}/mmcmodules" CACHE PATH
    "Directory where built .mmco plugins are placed for runtime discovery")

file(MAKE_DIRECTORY "${KITE_PLUGIN_STAGING_DIR}")

# Bring in the signing helper so per-plugin CMakeLists.txt may opt in to
# mmco_sign_plugin() explicitly, and so we can sign every plugin in one
# shot when Kite_PLUGIN_SIGN_ALL is ON.
include(MMCOSign)

####### Staging plugins
if(Kite_STAGING_PLUGINS)
    add_subdirectory(staging)
endif()

#################################### Register plugins here ####################################

# Track every in-tree plugin target name so we can drive automatic
# signing at the end of this listfile.
#
# This list deliberately does NOT include staging plugins. Staging
# lives under plugins/staging/ and is gated behind the
# `Kite_STAGING_PLUGINS` cache option (off by default). Plugins
# graduate out of staging by moving their subdirectory back up one
# level and being re-listed below.
#
# Plugins can also graduate the other way — out of plugins/ and into
# the launcher itself. When that happens, drop the subdirectory here
# and add the module name to launcher/plugin/CoreSupersededPlugins.h so
# stale .mmco files in users' mmcmodules/ directories are refused
# instead of doubling up on the core feature.
#   - Filelink: moved into core
#   - PackPortal: moved into core
#   - SkinManager: moved into core
#   - BackupSystem: moved into core (launcher/backup + BackupPage)
#   - NewsViewer:   moved into core (launcher/ui/dialogs/NewsViewerDialog,
#                   multi-feed support folded into news/NewsChecker)
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

# Automatic signing is wired *inside each plugin's own CMakeLists.txt*
# at the end of its target definition.
#
# Why not loop here? CMake (3.27+) tightened the rules around
# add_custom_command(TARGET <name> POST_BUILD …): the call must come
# from the *same CMakeLists.txt that created the target*. Calling it
# from this aggregator file works on older CMakes but breaks the
# macOS toolchain (Xcode 26 ships a strict CMake). The per-plugin
# pattern is portable and avoids the policy change entirely.
#
# The list KITE_IN_TREE_PLUGIN_TARGETS is kept above for tooling /
# documentation that wants to know which targets exist, but no longer
# drives signing.

#################### Plugin runtime dependencies (Windows) ####################
#
# Neither of the launcher's two Windows deployment paths covers plugins:
#
#   * install(RUNTIME_DEPENDENCY_SET LAUNCHER_DEPENDENCY_SET) in
#     launcher/CMakeLists.txt only resolves the targets that joined that
#     set — and the install(TARGETS) rule in each plugin directory does
#     not.
#   * windeployqt is invoked on kite.exe alone, so it only ever sees
#     the Qt modules the launcher itself links (Core, Gui, Widgets,
#     Network, Xml, NetworkAuth, Concurrent).
#
# So a Qt module that *only* a plugin links never reaches the package,
# and the module dies at startup with LoadLibrary error 126
# (ERROR_MOD_NOT_FOUND) — the file is found, its dependency is not.
# That is exactly what happened to SkinManager, the only in-tree plugin
# pulling in Qt6::OpenGL / Qt6::OpenGLWidgets.
#
# $<TARGET_RUNTIME_DLLS:tgt> is precisely the list we need (the shared
# libraries a target links, transitively) and it covers every plugin
# registered above without each one having to remember to opt in.
#
# No equivalent copy is made for the build tree on purpose: nothing puts
# Qt DLLs next to build/<cfg>/kite.exe either, so running from the
# build tree resolves every Qt DLL — launcher and plugin alike —
# through PATH. Adding a plugins-only copy there would just be
# inconsistent.
if(WIN32)
    set(KITE_PLUGIN_RUNTIME_DLLS "")
    foreach(_plugin_target IN LISTS KITE_IN_TREE_PLUGIN_TARGETS)
        list(APPEND KITE_PLUGIN_RUNTIME_DLLS
            "$<TARGET_RUNTIME_DLLS:${_plugin_target}>")
    endforeach()
    unset(_plugin_target)

    if(KITE_PLUGIN_RUNTIME_DLLS)
        # Most of what this resolves (Qt6Core, Qt6Gui, Qt6Widgets,
        # Qt6Network) the launcher already installs, and re-installing an
        # identical file is a no-op. The point is the handful that it
        # does not.
        install(FILES ${KITE_PLUGIN_RUNTIME_DLLS}
            DESTINATION "${BINARY_DEST_DIR}")
    endif()
endif()
