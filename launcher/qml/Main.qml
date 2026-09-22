// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme
import MeshMC.Components

/*
 * Root of the QML user interface: sidebar, top bar, instance grid.
 *
 * Everything it shows arrives as a required property set by QmlShell, so a
 * missing model is a load error rather than an empty window. Nothing here
 * hard-codes a colour or a size; those come from MeshMC.Theme.
 */
ApplicationWindow {
    id: root

    /* The core's instances through InstanceFilterModel: naturally sorted,
     * grouped, and filtered live by filterText. */
    required property var instanceModel

    /* Selection by instance id (IdSelectionModel). Ids rather than rows,
     * because rows move whenever the filter or the sort changes. */
    required property var selection

    // Read by QmlModule_test to prove this component, and not some default,
    // was instantiated.
    readonly property string moduleName: "MeshMC"

    width: 1180
    height: 740
    minimumWidth: 760
    minimumHeight: 480
    title: "MeshMC"
    color: Theme.palette.canvas

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SidebarNav {
            Layout.fillHeight: true
            Layout.preferredWidth: 220
            items: [
                { id: "instances", icon: "▦", label: qsTr("Instances") },
                { id: "modpacks",  icon: "⬡", label: qsTr("Modpacks") },
                { id: "settings",  icon: "⚙", label: qsTr("Settings") }
            ]
            currentId: "instances"
            // Honest placeholders until accounts are wired in: with no
            // account signed in, the launcher really is running as a guest.
            accountName: qsTr("Guest")
            accountStatus: qsTr("Not signed in")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            TopBar {
                id: topBar

                Layout.fillWidth: true
                title: qsTr("Instances")
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                Button {
                    text: qsTr("New instance")
                    highlighted: true
                }
            }

            InstanceGrid {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: root.instanceModel
                onSelectedIdChanged: {
                    if (selectedId.length > 0)
                        root.selection.selectOnly(selectedId)
                    else
                        root.selection.clear()
                }
            }
        }
    }
}
