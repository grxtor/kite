// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme
import MeshMC.Components

ApplicationWindow {
    id: root

    required property var instanceModel
    required property var selection
    // QmlShell: actions (launchInstance, editInstance, ...), account summary
    // and the extra instance models the library needs.
    required property var shell

    readonly property string moduleName: "MeshMC"

    width: 1240
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    title: "MeshMC"
    color: Theme.palette.canvas

    property string selectedId: ""
    onSelectedIdChanged: {
        if (selectedId.length > 0)
            root.selection.selectOnly(selectedId)
        else
            root.selection.clear()
    }

    function call(name, arg) {
        if (root.shell && typeof root.shell[name] === "function")
            arg === undefined ? root.shell[name]() : root.shell[name](arg)
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: topBar.focusSearch()
    }
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: root.call("createInstance")
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SidebarNav {
            Layout.fillHeight: true
            Layout.preferredWidth: implicitWidth
            items: [
                { id: "library", icon: "library", label: qsTr("Library") },
                { id: "discover", icon: "compass", label: qsTr("Discover") }
            ]
            footerItems: [
                { id: "settings", icon: "settings", label: qsTr("Settings") }
            ]
            currentId: "library"
            recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
            accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
            accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
            accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
            onItemActivated: (id) => {
                // Discover and Settings still open the existing dialogs.
                if (id === "discover")
                    root.call("createInstance")
                else if (id === "settings")
                    root.call("openSettings")
            }
            onRecentActivated: (id) => root.selectedId = id
            onRecentPlayRequested: (id) => root.call("launchInstance", id)
            onAccountClicked: root.call("manageAccounts")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            TopBar {
                id: topBar
                Layout.fillWidth: true
                title: qsTr("Library")
                count: root.instanceModel && root.instanceModel.count !== undefined ? root.instanceModel.count : -1
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                Button {
                    text: qsTr("New instance")
                    highlighted: true
                    icon.source: Icons.url("plus")
                    onClicked: root.call("createInstance")
                }
            }

            LibraryPage {
                Layout.fillWidth: true
                Layout.fillHeight: true
                focus: true
                instanceModel: root.instanceModel
                recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
                heroModel: root.shell && root.shell.heroModel ? root.shell.heroModel : null
                sectionModelFor: function (group) {
                    return root.shell && typeof root.shell.sectionModel === "function"
                            ? root.shell.sectionModel(group) : null
                }
                searchText: topBar.searchText
                selectedId: root.selectedId

                onSelectRequested: (id) => root.selectedId = id
                onLaunchRequested: (id) => root.call("launchInstance", id)
                onStopRequested: (id) => root.call("killInstance", id)
                onEditRequested: (id) => root.call("editInstance", id)
                onFolderRequested: (id) => root.call("openInstanceFolder", id)
                onCreateRequested: root.call("createInstance")
                onClearSearchRequested: topBar.searchText = ""
            }
        }
    }
}
