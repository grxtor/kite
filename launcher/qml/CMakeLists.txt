# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## QML user interface module ########

# One CMakeLists per QML module, living in the directory that holds that
# module's .qml files. qt_add_qml_module places each file at
#   ${RESOURCE_PREFIX}/${URI as path}/${path relative to CMAKE_CURRENT_SOURCE_DIR}
# so a module declared from launcher/ with QML_FILES qml/Main.qml would land at
# qrc:/qt/qml/MeshMC/qml/Main.qml -- the source tree leaking into the import
# path. Declaring it here instead keeps the alias a bare Main.qml.
#
# Submodules follow the same rule: MeshMC.Theme gets launcher/qml/Theme/, so the
# directory layout mirrors the URI.
#
# RESOURCE_PREFIX is spelled out rather than relying on qt_policy(QTP0001),
# which is Qt 6.5+; the floor here is 6.4 (Debian 12, Ubuntu 24.04 LTS). Setting
# it by hand gives the same qrc:/qt/qml/<URI>/ layout on every supported Qt, so
# these URLs do not move when the floor is eventually raised.
qt_add_library(MeshMC_qml STATIC)

qt_add_qml_module(MeshMC_qml
    URI MeshMC
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Main.qml
)

target_link_libraries(MeshMC_qml PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
