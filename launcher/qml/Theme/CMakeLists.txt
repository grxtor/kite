# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

# Kite.Theme, the design-token QML module. Its own directory, so that
# qt_add_qml_module keeps the alias a bare Theme.qml.

# Must run before qt_add_qml_module: QT_QML_SINGLETON_TYPE is read while the
# module is generated, and setting it later would register Theme.qml as an
# ordinary type, giving every "import Kite.Theme" its own private Theme.
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_library(Kite_qml_theme STATIC)

qt_add_qml_module(Kite_qml_theme
    URI Kite.Theme
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Theme.qml
        # Here rather than in Kite.Style so Kite.Components can use them too.
        Shadow.qml
        GlassSurface.qml
        KiteShape.qml
    SOURCES
        ThemeService.h
        ThemeService.cpp
    # Inter 4.1 static TTFs (+ OFL.txt), loaded by Theme.qml's FontLoaders by
    # qrc path so Theme.font.family is real on every machine.
    RESOURCES
        fonts/inter/Inter-Regular.ttf
        fonts/inter/Inter-Medium.ttf
        fonts/inter/Inter-SemiBold.ttf
        fonts/inter/Inter-Bold.ttf
        fonts/inter/OFL.txt
        # Montserrat 9.000 static TTFs, the heading face.
        fonts/montserrat/Montserrat-Regular.ttf
        fonts/montserrat/Montserrat-SemiBold.ttf
        fonts/montserrat/Montserrat-Bold.ttf
        fonts/montserrat/OFL.txt
)

# ThemeService only reads ThemePalette (part of the core) and must not see
# QtWidgets or launcher/ui/, so this links Kite_core alone, never Kite_logic.
target_link_libraries(Kite_qml_theme PUBLIC
    Kite_core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Gui
)

# Proves the module resolves and its tokens read back: a shifted resource
# prefix, a qmlcachegen failure or the singleton registering as a plain type
# would otherwise pass a green build.
add_unit_test(Theme
    SOURCES Theme_test.cpp
    # Kite_qml_theme explicitly: the test counts ThemeService::changed from C++,
    # and qt_import_qml_plugins only brings in what QML itself reaches.
    LIBS Kite_qml_theme Kite_qml_themeplugin Kite_core Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Gui
    )

# The engine refuses the module without its QML plugin, and add_unit_test uses
# a plain add_executable, which never imports it. The plugin is also in LIBS
# above because this test's QML is inline, so qmlimportscanner never sees
# `import Kite.Theme`.
qt_import_qml_plugins(Theme_test)
