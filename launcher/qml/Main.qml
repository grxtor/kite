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
    // "library", "discover", "settings" or "instance".
    property string page: "library"
    // The instance the instance page shows.
    property string openedInstanceId: ""
    property var openedDetails: null

    function openInstance(id) {
        if (!root.shell || typeof root.shell.instanceDetails !== "function")
            return
        root.openedDetails = root.shell.instanceDetails(id)
        root.openedInstanceId = id
        root.selectedId = id
        root.page = "instance"
    }

    Component.onCompleted: {
        if (root.shell && root.shell.settings) {
            SettingsStore.adapter = root.shell.settings
            var mode = SettingsStore.string("UiThemeMode")
            if (mode === "dark" || mode === "light")
                Theme.mode = mode
        }
    }
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
    Binding {
        target: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
        property: "instanceId"
        value: root.openedInstanceId.length > 0 ? root.openedInstanceId : "/"
        when: !!root.shell && !!root.shell.instancePageModel
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
            currentId: root.page === "instance" ? "library" : root.page
            recentModel: root.shell && root.shell.recentModel ? root.shell.recentModel : null
            accountName: root.shell && root.shell.accountName ? root.shell.accountName : ""
            accountKind: root.shell && root.shell.accountKind ? root.shell.accountKind : ""
            accountAvatarSource: root.shell && root.shell.accountFace ? root.shell.accountFace : ""
            onItemActivated: (id) => root.page = id
            onRecentActivated: (id) => {
                root.page = "library"
                root.selectedId = id
            }
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
                // The instance page has its own banner and way back.
                visible: root.page !== "instance"
                title: root.page === "settings" ? qsTr("Settings")
                     : root.page === "discover" ? qsTr("Discover") : qsTr("Library")
                count: root.page === "library" && root.instanceModel && root.instanceModel.count !== undefined
                       ? root.instanceModel.count : -1
                searchVisible: root.page === "library"
                searchPlaceholder: qsTr("Search instances")
                onSearchTextChanged: root.instanceModel.filterText = searchText

                Button {
                    visible: root.page === "library"
                    text: qsTr("New instance")
                    highlighted: true
                    icon.source: Icons.url("plus")
                    onClicked: root.call("createInstance")
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // A page's own minimum must never widen the window past
                // what it is; pages lay themselves out in what they get.
                Layout.minimumWidth: 0
                currentIndex: ["library", "settings", "discover", "instance"].indexOf(root.page)

                LibraryPage {
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
                    onEditRequested: (id) => root.openInstance(id)
                    onFolderRequested: (id) => root.call("openInstanceFolder", id)
                    onCreateRequested: root.call("createInstance")
                    onClearSearchRequested: topBar.searchText = ""
                }

                SettingsPage {
                    systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                    onOpenClassicRequested: (page) => root.call("openSettings", page)
                    onOpenPathRequested: (path) => root.call("openPath", path)
                }

                DiscoverPage {
                    model: root.shell && root.shell.modpackModel ? root.shell.modpackModel : null
                    installer: function (projectId, versionId, name, group) {
                        return root.shell.installModpack(projectId, versionId, name, group)
                    }
                    onShowInstanceRequested: (id) => {
                        root.page = "library"
                        if (id.length > 0)
                            root.selectedId = id
                    }
                    onOtherPlatformsRequested: root.call("createInstance")
                }

                InstancePage {
                    headerModel: root.shell && root.shell.instancePageModel ? root.shell.instancePageModel : null
                    details: root.openedDetails
                    systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                    onBackRequested: root.page = "library"
                    onLaunchRequested: (id) => root.call("launchInstance", id)
                    onStopRequested: (id) => root.call("killInstance", id)
                    onClassicEditorRequested: (id) => root.call("editInstance", id)
                    onOpenPathRequested: (path) => root.call("openPath", path)
                }
            }
        }
    }
}
