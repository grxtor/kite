# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

######## Kite.Skin3D: the optional 3D skin preview ########

# Only ever added from launcher/qml/CMakeLists.txt's
# `if(Kite_ENABLE_SKIN3D) add_subdirectory(Skin3D)`, itself gated on the
# top-level CMakeLists.txt's find_package(... OPTIONAL_COMPONENTS Quick3D)
# -- so by the time this file runs, Qt::Quick3D is known to exist. Same
# per-module-CMakeLists shape as Theme/Style/Components: RESOURCE_PREFIX
# spelled out by hand (the 6.4 floor predates qt_policy(QTP0001)), and this
# file lives beside the .qml it lists so the URI (Kite.Skin3D) and the
# on-disk path agree.
qt_add_library(Kite_qml_skin3d STATIC)

qt_add_qml_module(Kite_qml_skin3d
    URI Kite.Skin3D
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    # Not IMPORTS: SkinPreview3D.qml `import QtQuick3D` itself.
    # DEPENDENCIES instead, the same way Style/CMakeLists.txt lists
    # QtQuick.Templates, so this module's own qmldir records the runtime
    # dependency for tooling that reads it (qmlimportscanner already finds
    # both from the `import` statements regardless -- see the packaging
    # comments in launcher/CMakeLists.txt).
    DEPENDENCIES
        QtQuick3D
        QtQuick3D.Helpers
    # SkinPreview3D.qml: the 3D player in the skin editor and on the
    # Accounts page (Components/SkinPreview.qml loads it). It has a module
    # of its own because this is the module that only exists when Qt
    # Quick3D does.
    QML_FILES
        SkinPreview3D.qml
)

target_link_libraries(Kite_qml_skin3d PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Quick3D
)
