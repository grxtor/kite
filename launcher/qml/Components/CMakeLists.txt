# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## Launcher-specific QML building blocks (MeshMC.Components) ########

# Mirrors launcher/qml/CMakeLists.txt: RESOURCE_PREFIX is spelled out by hand
# rather than left to qt_policy(QTP0001) (Qt 6.5+), since the floor here is
# 6.4, and this file lives beside the .qml it lists so the URI (MeshMC.Components)
# and the on-disk path agree.
#
# Not added to the parent CMakeLists here -- the integrator wires this
# library into the MeshMC_qml target once MeshMC.Theme exists to link against.
qt_add_library(MeshMC_qml_components STATIC)

qt_add_qml_module(MeshMC_qml_components
    URI MeshMC.Components
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        InstanceCard.qml
        InstanceGrid.qml
        EmptyState.qml
        SidebarNav.qml
        NavItem.qml
        AccountChip.qml
        TopBar.qml
        SectionHeader.qml
        StatusBadge.qml
        Gallery.qml
    IMPORTS
        MeshMC.Theme
)

target_link_libraries(MeshMC_qml_components PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
