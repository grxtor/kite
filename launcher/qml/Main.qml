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
    // "library", "discover", "settings", "instance" or "accounts".
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

    function openNewInstance() {
        if (!root.shell || !root.shell.newInstance)
            return root.call("createInstance")
        newInstanceDialog.controller = root.shell.newInstance
        newInstanceDialog.open()
    }

    function instanceGroups() {
        var groups = root.shell && root.shell.groups ? root.shell.groups : []
        return [""].concat(groups.filter(g => g.length > 0))
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
        onActivated: root.openNewInstance()
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
            currentId: root.page === "instance" ? "library" : root.page === "accounts" ? "" : root.page
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
            onAccountClicked: root.page = "accounts"
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
                     : root.page === "discover" ? qsTr("Discover")
                     : root.page === "accounts" ? qsTr("Accounts") : qsTr("Library")
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
                    onClicked: root.openNewInstance()
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // A page's own minimum must never widen the window past
                // what it is; pages lay themselves out in what they get.
                Layout.minimumWidth: 0
                currentIndex: ["library", "settings", "discover", "instance", "accounts"].indexOf(root.page)

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
                    onCreateRequested: root.openNewInstance()
                    onRenameRequested: (id, name) => {
                        renameDialog.targetId = id
                        renameDialog.value = name
                        renameDialog.open()
                    }
                    onIconRequested: (id, iconKey) => {
                        iconDialog.targetId = id
                        iconDialog.current = iconKey
                        iconDialog.open()
                    }
                    onGroupRequested: (id, group) => {
                        groupDialog.targetId = id
                        groupDialog.value = group
                        groupDialog.suggestions = root.instanceGroups()
                        groupDialog.open()
                    }
                    onDuplicateRequested: (id, name, group) => {
                        duplicateDialog.targetId = id
                        duplicateDialog.targetGroup = group
                        duplicateDialog.value = qsTr("%1 (copy)").arg(name)
                        duplicateDialog.open()
                    }
                    onDeleteRequested: (id, name) => {
                        deleteDialog.targetId = id
                        deleteDialog.text = qsTr("Delete \u201c%1\u201d? Its folder \u2014 worlds, mods, screenshots \u2014 is removed for good.").arg(name)
                        deleteDialog.open()
                    }
                    onClearSearchRequested: topBar.searchText = ""
                }

                SettingsPage {
                    systemMemoryMiB: root.shell && root.shell.systemMemoryMiB ? root.shell.systemMemoryMiB : 8192
                    onOpenClassicRequested: (page) => {
                        if (page === "accounts")
                            root.page = "accounts"
                        else
                            root.call("openSettings", page)
                    }
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

                AccountsPage {
                    controller: root.shell && root.shell.accountsController ? root.shell.accountsController : null
                }
            }
        }
    }

    NewInstanceDialog {
        id: newInstanceDialog
        onMoreWaysRequested: root.call("createInstance")
        onCreated: root.page = "library"
    }

    // Instance management, from the library's menu.
    PromptDialog {
        id: renameDialog
        property string targetId
        title: qsTr("Rename instance")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.shell.renameInstance(targetId, text))
                close()
            else
                error = qsTr("That name cannot be used.")
        }
    }

    PromptDialog {
        id: groupDialog
        property string targetId
        title: qsTr("Move to group")
        label: qsTr("Type a new group or pick an existing one.")
        placeholder: qsTr("No group")
        confirmText: qsTr("Move")
        allowEmpty: true
        onSubmitted: (text) => {
            root.shell.setInstanceGroup(targetId, text)
            close()
        }
    }

    PromptDialog {
        id: duplicateDialog
        property string targetId
        property string targetGroup
        property var watcher: null
        title: qsTr("Duplicate instance")
        label: qsTr("A full copy, worlds included, under a new name.")
        confirmText: qsTr("Duplicate")
        onSubmitted: (text) => {
            watcher = root.shell.duplicateInstance(targetId, text, targetGroup)
            close()
            if (watcher)
                toast.show(qsTr("Copying \u201c%1\u201d\u2026").arg(text))
            else
                toast.show(qsTr("The instance could not be copied."), "danger")
        }
        Connections {
            target: duplicateDialog.watcher
            ignoreUnknownSignals: true
            function onFinished(ok) {
                toast.show(ok ? qsTr("Copy ready in your library.")
                              : qsTr("Copy failed: %1").arg(duplicateDialog.watcher.error),
                           ok ? "success" : "danger")
            }
        }
    }

    ConfirmDialog {
        id: deleteDialog
        property string targetId
        title: qsTr("Delete instance")
        confirmText: qsTr("Delete instance")
        onConfirmed: {
            if (root.shell.deleteInstance(targetId)) {
                if (root.selectedId === targetId)
                    root.selectedId = ""
                toast.show(qsTr("Instance deleted."), "success")
            } else {
                toast.show(qsTr("The instance could not be deleted. Is it running?"), "danger")
            }
        }
    }

    IconPickerDialog {
        id: iconDialog
        property string targetId
        iconsModel: root.shell && root.shell.iconsModel ? root.shell.iconsModel : null
        onPicked: (key) => root.shell.setInstanceIcon(targetId, key)
        onOpenFolderRequested: root.call("openPath", root.shell.iconsDir || "icons")
    }

    Toast {
        id: toast
    }
}
