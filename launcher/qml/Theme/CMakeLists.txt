# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## MeshMC.Theme: the design-token QML module ########

# Its own directory, one level below launcher/qml/, for the same reason
# MeshMC itself sits in launcher/qml/ rather than launcher/: qt_add_qml_module
# places files at ${RESOURCE_PREFIX}/${URI as path}/..., so declaring the
# module from the directory that mirrors "MeshMC/Theme" keeps the alias a bare
# Theme.qml instead of the source tree leaking into the import path. Not added
# to the parent CMakeLists here -- the integrator wires in add_subdirectory().

# Has to run before qt_add_qml_module: QT_QML_SINGLETON_TYPE is read while the
# module is generated, and setting it after would leave Theme.qml registered
# as an ordinary (re-instantiable) type, silently handing every "import
# MeshMC.Theme" its own private Theme instead of the one every component is
# meant to share.
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(MeshMC_qml_theme STATIC)

qt_add_qml_module(MeshMC_qml_theme
    URI MeshMC.Theme
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Theme.qml
    SOURCES
        ThemeService.h
        ThemeService.cpp
    # Inter 4.1 static TTFs (+ their OFL.txt), bundled here rather than as a
    # loose file on disk: Theme.qml's own FontLoader instances load them by
    # qrc path so Theme.font.family is real on every machine, not only one
    # that happens to already have Inter installed system-wide. Living next
    # to Theme.qml keeps the module and the asset it needs to function in one
    # place -- see the comment on Theme.qml's font.family.
    RESOURCES
        fonts/inter/Inter-Regular.ttf
        fonts/inter/Inter-Medium.ttf
        fonts/inter/Inter-SemiBold.ttf
        fonts/inter/Inter-Bold.ttf
        fonts/inter/OFL.txt
)

# ThemeService only reads ThemePalette (theme/ThemePalette.h, part of the
# core); it must not see QtWidgets or launcher/ui/, so this links MeshMC_core
# alone, never MeshMC_logic.
target_link_libraries(MeshMC_qml_theme PUBLIC
    MeshMC_core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Gui
)

######## Unit test ########

# Same shape as qml/QmlModule_test.cpp: proves the module resolves and its
# tokens read back the values declared above -- a shifted resource prefix, a
# qmlcachegen failure or the singleton registering as a plain type would all
# otherwise pass a green build silently.
add_unit_test(Theme
    SOURCES Theme_test.cpp
    # MeshMC_qml_theme explicitly: the test counts ThemeService::changed from
    # C++, and qt_import_qml_plugins only brings in what QML itself reaches.
    LIBS MeshMC_qml_theme MeshMC_qml_themeplugin MeshMC_core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Gui
    )

# qt_add_qml_module builds the module as a static library plus a separate QML
# plugin, and the engine refuses the module without it; add_unit_test uses a
# plain add_executable, which -- unlike qt_add_executable -- never imports it
# on its own. Same reasoning as QmlModule_test in launcher/CMakeLists.txt.
#
# The plugin target is also listed in LIBS above, explicitly: this test's QML
# is inline (QQmlComponent::setData), so qmlimportscanner never sees the
# `import MeshMC.Theme` and cannot work out on its own that it is needed.
qt_import_qml_plugins(Theme_test)
