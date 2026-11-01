# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

# Kite.Skin3D, the optional 3D skin preview. Only added when Kite_ENABLE_SKIN3D
# is on (launcher/qml/CMakeLists.txt), so Qt::Quick3D is known to exist here.
qt_add_library(Kite_qml_skin3d STATIC)

qt_add_qml_module(Kite_qml_skin3d
    URI Kite.Skin3D
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    # DEPENDENCIES, not IMPORTS, so this module's qmldir records the runtime
    # dependency for tooling that reads it.
    DEPENDENCIES
        QtQuick3D
        QtQuick3D.Helpers
    # Loaded by Components/SkinPreview.qml. Its own module because it only
    # exists when Qt Quick3D does.
    QML_FILES
        SkinPreview3D.qml
)

target_link_libraries(Kite_qml_skin3d PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Quick3D
)
