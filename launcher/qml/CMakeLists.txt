# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

######## QML user interface module ########

# One CMakeLists per QML module, living in the directory that holds that
# module's .qml files. qt_add_qml_module places each file at
#   ${RESOURCE_PREFIX}/${URI as path}/${path relative to CMAKE_CURRENT_SOURCE_DIR}
# so a module declared from launcher/ with QML_FILES qml/Main.qml would land at
# qrc:/qt/qml/Kite/qml/Main.qml -- the source tree leaking into the import
# path. Declaring it here instead keeps the alias a bare Main.qml.
#
# Submodules follow the same rule: Kite.Theme gets launcher/qml/Theme/, so the
# directory layout mirrors the URI.
#
# RESOURCE_PREFIX is spelled out rather than relying on qt_policy(QTP0001),
# which is Qt 6.5+; the floor here is 6.4 (Debian 12, Ubuntu 24.04 LTS). Setting
# it by hand gives the same qrc:/qt/qml/<URI>/ layout on every supported Qt, so
# these URLs do not move when the floor is eventually raised.
# The design system, one module per concern, each in its own directory for the
# same resource-alias reason as this one. Declared first because the modules
# below import them.
add_subdirectory(Theme)
add_subdirectory(Style)
add_subdirectory(Components)

# The 3D skin preview, only when Qt Quick3D was found (see the top-level
# CMakeLists.txt's find_package(... OPTIONAL_COMPONENTS Quick3D) and the
# Kite_ENABLE_SKIN3D option next to it). Kept out of the unconditional list
# above so a Quick3D-less build never even sees Skin3D/'s CMakeLists.txt.
if(Kite_ENABLE_SKIN3D)
    add_subdirectory(Skin3D)
endif()

qt_add_library(Kite_qml STATIC)

qt_add_qml_module(Kite_qml
    URI Kite
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Main.qml
)

# The C++ that drives the QML: the engine owner and whatever it hands over.
target_sources(Kite_qml PRIVATE
    QmlShell.h
    QmlShell.cpp
    QmlUiHost.h
    QmlUiHost.cpp
    InstanceIconProvider.h
    InstanceIconProvider.cpp
    AccountFaceProvider.h
    AccountFaceProvider.cpp
    ScreenshotThumbnailProvider.h
    ScreenshotThumbnailProvider.cpp
    ModIconProvider.h
    ModIconProvider.cpp
)

# The QML side reads the core's models. It does not see QtWidgets: Kite_core
# cannot link them, and nothing here asks for them.
target_link_libraries(Kite_qml PUBLIC
    Kite_qml_theme
    Kite_qml_style
    Kite_qml_components
    Kite_core
)

# The plugin targets, and not just the modules' backing libraries, have to reach
# the executable: qt_import_qml_plugins only imports plugins it finds in the
# link closure, and a module whose plugin is missing fails at load time with
# 'plugin "..." not found' -- after a perfectly green build. PUBLIC so they
# propagate from here to whatever links Kite_qml.
target_link_libraries(Kite_qml PUBLIC
    Kite_qml_themeplugin
    Kite_qml_styleplugin
    Kite_qml_componentsplugin
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)

# The 3D skin preview's module and plugin, linked the same way as the three
# above, plus the compile-time flag QmlShell.cpp reads to tell QML whether
# SkinPreview has a 3D view to load at all -- see QmlShell::rootProperties().
# Both stay entirely out of a build with Kite_ENABLE_SKIN3D off.
if(Kite_ENABLE_SKIN3D)
    target_link_libraries(Kite_qml PUBLIC
        Kite_qml_skin3d
        Kite_qml_skin3dplugin
    )
    target_compile_definitions(Kite_qml PRIVATE KITE_HAS_SKIN3D=1)
endif()
