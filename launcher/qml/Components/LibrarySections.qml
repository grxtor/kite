// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The library's instances: every group as its own collapsible section
 * (InstanceSection), the instance ⋯ menu (InstanceMenu), the group dialogs
 * and the Export/Create shortcut flows. It has no scrolling or page frame
 * of its own, so two pages can share it: LibraryPage puts it in a Flickable
 * on its own, and HomePage puts it under "Continue playing" in the same
 * Flickable.
 *
 * Its height is its content's. The host hands it the Flickable it sits in,
 * so dragging an instance towards the edge of the view scrolls that view.
 *
 * Without `sectionModelFor` there are no per-group models to show, and the
 * whole (search-filtered) `instanceModel` is one section without a header --
 * as it is when the library is set to show no groups (`instanceModel`'s
 * `grouped` off, which orders it without them).
 */
Item {
    id: root

    property var instanceModel: null
    // group -> that group's search-filtered model (QmlShell.sectionModel).
    property var sectionModelFor: null
    property string searchText: ""
    property string selectedId: ""
    // "grid" (InstanceCard tiles) or "list" (dense InstanceListRow rows).
    property string viewMode: "grid"
    // The view this sits in, for scrolling while an instance is dragged.
    property Flickable flickable: null

    // The ⋯ menu's Play options (LaunchOptionsMenu.qml): what it offers comes
    // from launchOptionsFor(id) -- Main.qml's launchOptions() -- and its
    // choices leave as the signals below.
    property var launchOptionsFor: null
    // id -> export / shortcut controller (QmlShell, set in Main.qml). The ⋯
    // menu offers "Export…" and "Create shortcut…" once these are set.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    // LibraryController (QmlShell's `library`): group management, folds
    // that persist, dragging an instance onto a group, the instance's
    // folders and Duplicate with options. Until it is set the sections keep
    // session-only folds and Duplicate goes out as duplicateRequested.
    property var library: null

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal editRequested(string id)
    signal folderRequested(string id)
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal navigateRequested(string target)

    // Modrinth App's grid: a 12px gap between cards.
    readonly property int gutter: Theme.space.md
    // Cards grow to fill the row between these, adding a column once there
    // is room for one more at the minimum -- so the row is always full
    // width, never a strip of small cards with the rest of it empty.
    // Modrinth's are 160-180 wide, mostly their picture.
    readonly property int minCardWidth: 168
    readonly property int maxCardWidth: 200
    readonly property int columns: root.columnsFor(root.width)
    readonly property real cardWidth: Math.floor((root.width - root.gutter * (root.columns - 1)) / root.columns)

    readonly property bool searching: root.searchText.length > 0
    readonly property bool grouped: typeof root.sectionModelFor === "function"
                                    && !(root.instanceModel && root.instanceModel.grouped === false)
    readonly property var groups: root.instanceModel && root.instanceModel.groups ? root.instanceModel.groups : []

    // An instance being dragged to another group: the host's view holds
    // still under it (see `flickable`), and scrolls when the pointer nears
    // its top or bottom edge so a group out of view can still be reached.
    property bool dragging: false
    property real dragSceneY: 0
    readonly property int autoScrollEdge: Theme.space.xxl * 2
    readonly property int autoScrollStep: Theme.space.md
    readonly property int autoScrollIntervalMs: 16

    implicitHeight: sectionsColumn.implicitHeight

    function columnsFor(width) {
        var columns = Math.max(1, Math.floor((width + root.gutter) / (root.minCardWidth + root.gutter)))
        while ((width - root.gutter * (columns - 1)) / columns > root.maxCardWidth)
            columns++
        return columns
    }

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

    function exportInstance(id, format) {
        flows.exportInstance(id, format)
    }
    function createShortcut(id) {
        flows.createShortcut(id)
    }
    function duplicateInstance(id) {
        var info = root.library ? root.library.describe(id) : ({})
        flows.duplicateInstance(id, info.name || id, info.group || "", info.iconKey || "")
    }
    // A group's ⋯ menu, for review snapshots (Main.qml's "groupmenu=<name>").
    function openGroupMenu(group) {
        for (var i = 0; i < sections.count; ++i) {
            var section = sections.itemAt(i)
            if (section && section.group === group && section.manageable)
                section.openGroupMenu()
        }
    }
    // The instance ⋯ menu for @p id, under @p anchor -- the cards' own, and
    // HomePage's "Continue playing" rows', which pass the @p name and
    // @p iconKey they already show for when there is no `library` to
    // describe the instance.
    // Without an anchor (a review snapshot has no mouse to open it at) it
    // opens at the top-left of the cards.
    function openInstanceMenu(id, running, anchor, name, iconKey) {
        var info = root.library ? root.library.describe(id) : ({})
        instanceMenu.openFor(id, !!running, info.name || name || "", info.iconKey || iconKey || "",
                             info.group || "", anchor || null)
        if (!anchor) {
            instanceMenu.x = 0
            instanceMenu.y = 0
        }
    }

    Column {
        id: sectionsColumn
        width: root.width
        spacing: Theme.space.xl

        Repeater {
            id: sections
            model: root.grouped ? root.groups : []
            delegate: InstanceSection {
                required property string modelData
                width: sectionsColumn.width
                title: modelData.length > 0 ? modelData : qsTr("Ungrouped")
                // Ungrouped instances get no header, as in MultiMC and
                // Prism: a named "Ungrouped" group read as one more group
                // the user had made.
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
                    groupDeleteDialog.text = qsTr("Delete the group “%1”? Its instances stay in your library, without a group.").arg(group)
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

        // No per-group models: every instance, in one section.
        InstanceSection {
            visible: !root.grouped
            width: sectionsColumn.width
            showHeader: false
            model: root.grouped ? null : root.instanceModel
            columns: root.columns
            cardWidth: root.cardWidth
            gutter: root.gutter
            selectedId: root.selectedId
            viewMode: root.viewMode
            onSelectRequested: (id) => root.selectRequested(id)
            onLaunchRequested: (id) => root.launchRequested(id)
            onStopRequested: (id) => root.stopRequested(id)
            onMenuRequested: (id, running, name, iconKey, group, anchor) => instanceMenu.openFor(id, running, name, iconKey, group, anchor)
        }
    }

    InstanceMenu {
        id: instanceMenu
        launchOptionsFor: root.launchOptionsFor
        library: root.library
        exportFlows: flows
        onLaunchRequested: (id) => root.launchRequested(id)
        onStopRequested: (id) => root.stopRequested(id)
        onEditRequested: (id) => root.editRequested(id)
        onFolderRequested: (id) => root.folderRequested(id)
        onRenameRequested: (id, name) => root.renameRequested(id, name)
        onIconRequested: (id, iconKey) => root.iconRequested(id, iconKey)
        onGroupRequested: (id, group) => root.groupRequested(id, group)
        onDuplicateRequested: (id, name, group) => root.duplicateRequested(id, name, group)
        onDeleteRequested: (id, name) => root.deleteRequested(id, name)
        onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
        onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
        onProfilerChosen: (id, profilerKey) => root.profilerChosen(id, profilerKey)
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // "Export…" and "Create shortcut…": the dialogs and their toast.
    InstanceExportFlows {
        id: flows
        exportControllerFor: root.exportControllerFor
        shortcutControllerFor: root.shortcutControllerFor
        library: root.library
    }

    Timer {
        interval: root.autoScrollIntervalMs
        repeat: true
        running: root.dragging && !!root.flickable
        onTriggered: {
            var view = root.flickable
            var y = view.mapFromItem(null, 0, root.dragSceneY).y
            var step = y < root.autoScrollEdge ? -root.autoScrollStep
                     : y > view.height - root.autoScrollEdge ? root.autoScrollStep : 0
            if (step !== 0)
                view.contentY = Math.max(0, Math.min(view.contentHeight - view.height, view.contentY + step))
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
