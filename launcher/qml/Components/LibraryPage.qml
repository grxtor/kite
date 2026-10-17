// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The instance library: every group as its own section. The instance most
 * likely to play next no longer gets its own hero card here -- the
 * persistent play bar (PlayDock, in Main.qml) replaces it, showing on every
 * page rather than just this one.
 *
 * The page never builds models itself. It is handed the search-filtered
 * instance model and a function that returns the model for one group -- all
 * C++ proxies, owned by the shell.
 */
Item {
    id: root

    property var instanceModel: null
    property var sectionModelFor: function (group) { return null }
    property string searchText: ""
    property string selectedId: ""
    // "grid" (InstanceCard tiles) or "list" (dense InstanceListRow rows);
    // session-only -- there is no persisted setting for it yet.
    property string viewMode: "grid"

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal editRequested(string id)
    signal folderRequested(string id)
    signal createRequested()
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)
    signal clearSearchRequested()

    // The ⋯ menu's Play options (LaunchOptionsMenu.qml): what it offers comes
    // from launchOptionsFor(id) -- Main.qml's launchOptions() -- and its
    // choices leave as the signals below.
    property var launchOptionsFor: null
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal navigateRequested(string target)

    // id -> export / shortcut controller (QmlShell, set in Main.qml). The ⋯
    // menu offers "Export…" and "Create shortcut…" once these are set; the
    // dialogs themselves are InstanceExportFlows below.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    // LibraryController (QmlShell's `library`, set in Main.qml): group
    // management, folds that persist, dragging an instance onto a group,
    // the instance's folders and Duplicate with options. Until it is set
    // the page keeps its session-only folds and Main.qml's duplicate.
    property var library: null

    function exportInstance(id, format) {
        exportFlows.exportInstance(id, format)
    }
    function createShortcut(id) {
        exportFlows.createShortcut(id)
    }
    // For review snapshots (Main.qml's "groupmenu=<name>" and
    // "duplicate=<id>" dev routes): a group's ⋯ menu, and the duplicate
    // dialog.
    function openGroupMenu(group) {
        for (var i = 0; i < sections.count; ++i) {
            var section = sections.itemAt(i)
            if (section && section.group === group && section.manageable)
                section.openGroupMenu()
        }
    }
    function duplicateInstance(id) {
        var info = root.library ? root.library.describe(id) : ({})
        exportFlows.duplicateInstance(id, info.name || id, info.group || "", info.iconKey || "")
    }
    // The card ⋯ menu for instance @p id, for review snapshots (Main.qml's
    // "librarymenu=<id>" dev route). A snapshot has no mouse to open it at,
    // so it goes to the top-left of the cards instead of the window corner.
    function openInstanceMenu(id) {
        var info = root.library ? root.library.describe(id) : ({})
        instanceMenu.openFor(id, false, info.name || "", info.iconKey || "", info.group || "")
        instanceMenu.x = root.contentX
        instanceMenu.y = Theme.space.xl
    }

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int gutter: Theme.space.lg
    // Cards grow to fill the row between these, adding a column once there
    // is room for one more at the minimum -- so the row is always full
    // width, never a strip of small cards with the rest of it empty.
    readonly property int minCardWidth: 220
    readonly property int maxCardWidth: 320
    // On a wide window the content stops at Theme.layout.maxContentWidth
    // and centres (TopBar uses the same rule, so its title and toolbar stay
    // over the cards' left edge).
    readonly property int contentWidth: Math.max(0, Math.min(flick.width - pagePadding * 2, Theme.layout.maxContentWidth))
    readonly property int contentX: Math.max(pagePadding, Math.round((flick.width - contentWidth) / 2))
    readonly property int columns: columnsFor(contentWidth)
    readonly property real cardWidth: Math.floor((contentWidth - gutter * (columns - 1)) / columns)

    readonly property bool searching: searchText.length > 0
    readonly property var groups: instanceModel && instanceModel.groups ? instanceModel.groups : []
    readonly property int instanceCount: instanceModel && instanceModel.count !== undefined ? instanceModel.count : 0

    // Which groups are folded away: the library's, kept in the group file,
    // or a session-only list without one.
    property var sessionCollapsed: []
    readonly property var collapsedGroups: root.library ? root.library.collapsedGroups : root.sessionCollapsed
    function setCollapsed(group, collapsed) {
        if (root.library) {
            root.library.setGroupCollapsed(group, collapsed)
            return
        }
        var next = root.sessionCollapsed.filter((g) => g !== group)
        if (collapsed)
            next.push(group)
        root.sessionCollapsed = next
    }

    // An instance being dragged to another group. The page holds still
    // under it, and scrolls when the pointer nears its top or bottom edge
    // so a group out of view can still be reached.
    property bool dragging: false
    property real dragSceneY: 0
    readonly property int autoScrollEdge: Theme.space.xxl * 2
    readonly property int autoScrollStep: Theme.space.md
    readonly property int autoScrollIntervalMs: 16

    function columnsFor(width) {
        var columns = Math.max(1, Math.floor((width + gutter) / (minCardWidth + gutter)))
        while ((width - gutter * (columns - 1)) / columns > maxCardWidth)
            columns++
        return columns
    }

    Keys.onEscapePressed: root.selectRequested("")

    // No page-wide photo wash behind the grid: the cards' own covers are
    // the artwork, and the page stays the flat canvas the top bar sits on,
    // so there is no edge where the two meet.

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.y + content.height + root.pagePadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: !root.dragging

        ScrollBar.vertical: ScrollBar {}

        // Clicking the empty page clears the selection.
        MouseArea {
            width: flick.width
            height: Math.max(flick.height, flick.contentHeight)
            onClicked: root.selectRequested("")
        }

        Column {
            id: content
            x: root.contentX
            y: Theme.space.xs
            width: root.contentWidth
            spacing: Theme.space.xl + Theme.space.sm

            Repeater {
                id: sections
                model: root.groups
                delegate: InstanceSection {
                    required property string modelData
                    width: content.width
                    title: modelData.length > 0 ? modelData : qsTr("Ungrouped")
                    // Ungrouped instances get no header, as in MultiMC and
                    // Prism: a named "Ungrouped" group read as one more
                    // group the user had made.
                    showHeader: modelData.length > 0
                    group: modelData
                    model: root.sectionModelFor(modelData)
                    columns: root.columns
                    cardWidth: root.cardWidth
                    gutter: root.gutter
                    selectedId: root.selectedId
                    viewMode: root.viewMode
                    // Without a header there is nothing to unfold it with.
                    collapsed: !root.searching && modelData.length > 0 && root.collapsedGroups.indexOf(modelData) >= 0
                    onToggleRequested: root.setCollapsed(modelData, root.collapsedGroups.indexOf(modelData) < 0)
                    manageable: !!root.library && modelData.length > 0
                    // Somewhere else to drop it only with two groups or more.
                    dragEnabled: !!root.library && root.groups.length > 1
                    onRenameGroupRequested: (group) => {
                        groupRenameDialog.group = group
                        groupRenameDialog.value = group
                        groupRenameDialog.open()
                    }
                    onDeleteGroupRequested: (group) => {
                        groupDeleteDialog.group = group
                        groupDeleteDialog.text = qsTr("Delete the group \u201c%1\u201d? Its instances stay in your library, without a group.").arg(group)
                        groupDeleteDialog.open()
                    }
                    onInstanceDropped: (id, group) => root.library.moveToGroup(id, group)
                    onDragActiveChanged: (active) => root.dragging = active
                    onDragMoved: (sceneY) => root.dragSceneY = sceneY
                    onSelectRequested: (id) => root.selectRequested(id)
                    onLaunchRequested: (id) => root.launchRequested(id)
                    onStopRequested: (id) => root.stopRequested(id)
                    onMenuRequested: (id, running, name, iconKey, group, anchor) => instanceMenu.openFor(id, running, name, iconKey, group, anchor)
                }
            }

        }
    }

    // The last visible row fades out at the play bar instead of being cut.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }

    EmptyState {
        anchors.centerIn: parent
        visible: root.instanceCount === 0
        title: root.searching ? qsTr("No instances match “%1”").arg(root.searchText)
                              : qsTr("No instances yet")
        body: root.searching ? qsTr("Check the spelling, or search for part of the name.")
                             : qsTr("An instance is one Minecraft setup: a version, a mod loader and its mods. Create one to start playing.")
        actionText: root.searching ? qsTr("Clear search") : qsTr("Create instance")
        actionIcon: root.searching ? "x" : "plus"
        onActionTriggered: root.searching ? root.clearSearchRequested() : root.createRequested()

        MeshIcon {
            iconName: root.searching ? "search" : "cube"
            size: 40
            color: Theme.palette.textTertiary
        }
    }

    Menu {
        id: instanceMenu

        property string targetId
        property bool targetRunning: false
        property string targetName
        property string targetIcon
        property string targetGroup
        // This instance's .minecraft and mods folders; empty where there is
        // no such folder to open (see LibraryController.folders()).
        property string gameFolder
        property string modsFolder

        function openFor(id, running, name, iconKey, group, anchor) {
            var folders = root.library ? root.library.folders(id) : ({})
            gameFolder = folders.game && folders.game !== folders.instance ? folders.game : ""
            modsFolder = folders.mods || ""
            targetId = id
            targetRunning = running
            targetName = name || ""
            targetIcon = iconKey || ""
            targetGroup = group || ""
            // Play options make no sense for a running instance. A submenu
            // has no `visible` of its own: its entry is a MenuItem Qt makes,
            // found here by its subMenu.
            var offerOptions = !running && !!root.launchOptionsFor
            for (var i = 0; i < instanceMenu.count; ++i) {
                var item = instanceMenu.itemAt(i)
                if (item && item.subMenu === playOptionsMenu) {
                    item.visible = offerOptions
                    item.height = offerOptions ? item.implicitHeight : 0
                    // An icon like its neighbours', or its label would
                    // start where theirs' icons do.
                    item.icon.source = Icons.url("sliders")
                }
            }
            if (!anchor) {
                popup()
                return
            }
            // Under the ⋯ button and right-aligned to it, like the instance
            // page's own ⋯, and held there while the menu settles its width.
            popup(anchor, anchor.width - instanceMenu.width, anchor.height + Theme.space.xs)
            instanceMenu.x = Qt.binding(() => anchor.width - instanceMenu.width)
        }

        MenuItem {
            text: instanceMenu.targetRunning ? qsTr("Stop") : qsTr("Play")
            icon.source: Icons.url(instanceMenu.targetRunning ? "stop" : "play")
            onTriggered: instanceMenu.targetRunning ? root.stopRequested(instanceMenu.targetId)
                                                    : root.launchRequested(instanceMenu.targetId)
        }
        LaunchOptionsMenu {
            id: playOptionsMenu
            instanceId: instanceMenu.targetId
            optionsFor: root.launchOptionsFor
            onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
            onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
            onProfilerChosen: (id, key) => root.profilerChosen(id, key)
            onNavigateRequested: (target) => root.navigateRequested(target)
        }
        // An arrow, "go to its page": the gear it had read as "settings".
        MenuItem {
            text: qsTr("Open")
            icon.source: Icons.url("arrow-right")
            onTriggered: root.editRequested(instanceMenu.targetId)
        }
        MenuItem {
            text: qsTr("Open folder")
            icon.source: Icons.url("folder")
            onTriggered: root.folderRequested(instanceMenu.targetId)
        }
        MenuItem {
            // A hidden item still takes its row in a Menu; zero it as well.
            visible: instanceMenu.gameFolder.length > 0
            height: visible ? implicitHeight : 0
            text: qsTr("Open .minecraft folder")
            icon.source: Icons.url("folder")
            onTriggered: root.library.openFolder(instanceMenu.gameFolder)
        }
        MenuItem {
            visible: instanceMenu.modsFolder.length > 0
            height: visible ? implicitHeight : 0
            text: qsTr("Open mods folder")
            icon.source: Icons.url("folder")
            onTriggered: root.library.openFolder(instanceMenu.modsFolder)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Rename\u2026")
            icon.source: Icons.url("edit")
            onTriggered: root.renameRequested(instanceMenu.targetId, instanceMenu.targetName)
        }
        MenuItem {
            text: qsTr("Change icon\u2026")
            icon.source: Icons.url("image")
            onTriggered: root.iconRequested(instanceMenu.targetId, instanceMenu.targetIcon)
        }
        // The instance page's ⋯ menu words it the same way.
        MenuItem {
            text: qsTr("Change group\u2026")
            icon.source: Icons.url("folder-input")
            onTriggered: root.groupRequested(instanceMenu.targetId, instanceMenu.targetGroup)
        }
        MenuItem {
            text: qsTr("Duplicate\u2026")
            icon.source: Icons.url("copy")
            onTriggered: exportFlows.canDuplicate
                         ? exportFlows.duplicateInstance(instanceMenu.targetId, instanceMenu.targetName,
                                                         instanceMenu.targetGroup, instanceMenu.targetIcon)
                         : root.duplicateRequested(instanceMenu.targetId, instanceMenu.targetName, instanceMenu.targetGroup)
        }
        MenuItem {
            // A hidden item still takes its row in a Menu; zero it as well.
            visible: exportFlows.canExport
            height: visible ? implicitHeight : 0
            text: qsTr("Export\u2026")
            icon.source: Icons.url("archive")
            onTriggered: exportFlows.exportInstance(instanceMenu.targetId)
        }
        MenuItem {
            visible: exportFlows.canCreateShortcut
            height: visible ? implicitHeight : 0
            text: qsTr("Create shortcut\u2026")
            icon.source: Icons.url("external-link")
            onTriggered: exportFlows.createShortcut(instanceMenu.targetId)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete\u2026")
            icon.source: Icons.url("trash")
            enabled: !instanceMenu.targetRunning
            onTriggered: root.deleteRequested(instanceMenu.targetId, instanceMenu.targetName)
        }
    }

    // "Export…" and "Create shortcut…": the dialogs and their toast.
    InstanceExportFlows {
        id: exportFlows
        exportControllerFor: root.exportControllerFor
        shortcutControllerFor: root.shortcutControllerFor
        library: root.library
    }

    Timer {
        interval: root.autoScrollIntervalMs
        repeat: true
        running: root.dragging
        onTriggered: {
            var y = flick.mapFromItem(null, 0, root.dragSceneY).y
            var step = y < root.autoScrollEdge ? -root.autoScrollStep
                     : y > flick.height - root.autoScrollEdge ? root.autoScrollStep : 0
            if (step !== 0)
                flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height, flick.contentY + step))
        }
    }

    PromptDialog {
        id: groupRenameDialog
        property string group
        title: qsTr("Rename group")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.library)
                root.library.renameGroup(group, text)
            close()
        }
    }

    ConfirmDialog {
        id: groupDeleteDialog
        property string group
        title: qsTr("Delete group")
        confirmText: qsTr("Delete group")
        // Nothing is lost: the instances stay, ungrouped.
        danger: false
        onConfirmed: if (root.library) root.library.deleteGroup(group)
    }
}
