# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

# One CMakeLists per QML module, in the directory that holds its .qml files:
# qt_add_qml_module places each file at
#   ${RESOURCE_PREFIX}/${URI as path}/${path relative to CMAKE_CURRENT_SOURCE_DIR}
# so declaring the module from launcher/ would land Main.qml at
# qrc:/qt/qml/Kite/qml/Main.qml. Submodules follow the same rule.
#
# RESOURCE_PREFIX is spelled out because qt_policy(QTP0001) needs Qt 6.5; the
# floor is 6.4 (Debian 12, Ubuntu 24.04 LTS).

# Declared first because the modules below import them.
add_subdirectory(Theme)
add_subdirectory(Style)
add_subdirectory(Components)

# The 3D skin preview, only when Qt Quick3D was found (Kite_ENABLE_SKIN3D).
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

# macOS: the title bar laid over the shell's top bar. AppKit comes with
# Kite_core.
if(APPLE)
    target_sources(Kite_qml PRIVATE
        MacWindowChrome.h
        MacWindowChrome.mm
    )
endif()

# The QML side reads the core's models and does not see QtWidgets.
target_link_libraries(Kite_qml PUBLIC
    Kite_qml_theme
    Kite_qml_style
    Kite_qml_components
    Kite_core
)

# The plugin targets, not just the backing libraries, have to reach the
# executable: qt_import_qml_plugins only imports plugins in the link closure,
# and a missing one fails at load time with 'plugin "..." not found' after a
# green build. PUBLIC so they propagate to whatever links Kite_qml.
target_link_libraries(Kite_qml PUBLIC
    Kite_qml_themeplugin
    Kite_qml_styleplugin
    Kite_qml_componentsplugin
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)

# The 3D skin preview's module and plugin, plus the flag QmlShell.cpp reads to
# tell QML whether SkinPreview has a 3D view (QmlShell::rootProperties()).
if(Kite_ENABLE_SKIN3D)
    target_link_libraries(Kite_qml PUBLIC
        Kite_qml_skin3d
        Kite_qml_skin3dplugin
    )
    target_compile_definitions(Kite_qml PRIVATE KITE_HAS_SKIN3D=1)
endif()
