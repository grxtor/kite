# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## MeshMC.Cat: the optional 3D cat companion ########

# Only ever added from launcher/qml/CMakeLists.txt's
# `if(MeshMC_ENABLE_CAT) add_subdirectory(Cat)`, itself gated on the
# top-level CMakeLists.txt's find_package(... OPTIONAL_COMPONENTS Quick3D)
# -- so by the time this file runs, Qt::Quick3D is known to exist. Same
# per-module-CMakeLists shape as Theme/Style/Components: RESOURCE_PREFIX
# spelled out by hand (the 6.4 floor predates qt_policy(QTP0001)), and this
# file lives beside the .qml it lists so the URI (MeshMC.Cat) and the
# on-disk path agree.
qt_add_library(MeshMC_qml_cat STATIC)

qt_add_qml_module(MeshMC_qml_cat
    URI MeshMC.Cat
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    # CatOverlay.qml `import MeshMC.Components` for SettingsStore (the
    # launcher-wide settings singleton -- CatEnabled/CatVariant/
    # UiReduceMotion are ordinary settings, read the same way any other QML
    # file reads them). Declared here, the same way Style/CMakeLists.txt
    # declares MeshMC.Theme, so this module's own build step can resolve
    # that import and the generated qmldir records the dependency.
    IMPORTS
        MeshMC.Components
    # Not IMPORTS: CatOverlay.qml/CatRig.qml `import QtQuick3D`,
    # `QtQuick.Window` and `QtQuick.Controls` themselves, the same way they
    # import MeshMC.Components above rather than relying on an implicit one.
    # DEPENDENCIES instead, the same way Style/CMakeLists.txt lists
    # QtQuick.Templates, so this module's own qmldir records the runtime
    # dependency for tooling that reads it (qmlimportscanner already finds
    # all three from the `import` statements regardless -- see the
    # packaging comments in launcher/CMakeLists.txt).
    DEPENDENCIES
        QtQuick3D
        QtQuick.Window
        QtQuick.Controls
    QML_FILES
        CatOverlay.qml
        CatRig.qml
    # The model's parts (generated from assets/cat.glb with Qt's balsam
    # tool -- see assets/CREDITS.md) and its coats. assets/cat.glb itself is
    # the source and is not needed at run time.
    RESOURCES
        model/meshes/object_0_mesh.mesh
        model/meshes/object_1_mesh.mesh
        model/meshes/object_2_mesh.mesh
        model/meshes/object_3_mesh.mesh
        model/meshes/object_4_mesh.mesh
        model/meshes/object_5_mesh.mesh
        model/meshes/object_6_mesh.mesh
        model/meshes/object_7_mesh.mesh
        model/meshes/object_8_mesh.mesh
        model/meshes/object_9_mesh.mesh
        model/meshes/object_10_mesh.mesh
        textures/cat_calico.png
        textures/cat_ginger.png
        textures/cat_black.png
        textures/cat_white.png
        textures/cat_siamese.png
        textures/heart.png
        textures/sleep_z.png
)

target_link_libraries(MeshMC_qml_cat PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Quick3D
    # For the `Overlay` attached type CatOverlay.qml reads (import
    # QtQuick.Controls) -- same reasoning as Style/CMakeLists.txt linking
    # this for the controls it imports directly.
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
