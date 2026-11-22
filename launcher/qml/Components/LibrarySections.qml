// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The library's instances: one grid of them all under the groups as filter chips (LibraryGroupChips), or, with
 * "Group by: Group", every group as a collapsible section; the instance ⋯ menu, the group dialogs and the
 * Export/Create shortcut flows. No scrolling of its own (Home's page scrolls it); its height is its content's.
 * Arrow keys move between the cards (LibraryKeys).
 */
Item {
    id: root

    property var instanceModel: null
    // group -> that group's search-filtered model (QmlShell.sectionModel).
    property var sectionModelFor: null
    property string searchText: ""
    property string selectedId: ""
    property string viewMode: "grid"
    property Flickable flickable: null

    // The ⋯ menu's Play options (LaunchOptionsMenu.qml); choices leave as the signals below.
    property var launchOptionsFor: null
    // id -> export / shortcut controller; the ⋯ menu offers "Export…" and "Create shortcut…" once set.
    property var exportControllerFor: null
    // QmlShell.shareCodes, for "Share as code…".
    property var shareCodes: null
    property var shortcutControllerFor: null
    // LibraryController: group management, persistent folds, group drops, folders and Duplicate
    // with options. Without it folds are session-only and Duplicate goes out as duplicateRequested.
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
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    readonly property int gutter: Theme.space.md
    // Cards grow to fill the row, adding a column once one more fits at this minimum.
    readonly property int minCardWidth: 168
    readonly property int maxCardWidth: 200
    readonly property int columns: root.columnsFor(root.width)
    readonly property real cardWidth: Math.floor((root.width - root.gutter * (root.columns - 1)) / root.columns)

    readonly property bool searching: root.searchText.length > 0
    readonly property bool grouped: typeof root.sectionModelFor === "function"
                                    && !(root.instanceModel && root.instanceModel.grouped === false)
    readonly property var groups: root.instanceModel && root.instanceModel.groups ? root.instanceModel.groups : []

    // The one grid's filter: "all" or "g:<group>" (LibraryGroupChips). Back to "all" when its group is gone,
    // deleted or with nothing left that answers the search.
    property string groupFilter: chips.allKey
    readonly property bool filtered: !root.grouped && chips.named.length > 0 && root.groupFilter !== chips.allKey
    onGroupsChanged: if (root.groupFilter !== chips.allKey && root.groups.indexOf(chips.groupOf(root.groupFilter)) < 0)
                         root.groupFilter = chips.allKey

    // An instance is being dragged to another group: the host's view holds still and scrolls near
    // its top or bottom edge.
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

    // Folded groups: the library's, kept in the group file, or session-only without one.
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
    function shareInstance(id, name) {
        flows.shareAsCode(id, name)
    }
    function duplicateInstance(id) {
        var info = root.library ? root.library.describe(id) : ({})
        flows.duplicateInstance(id, info.name || id, info.group || "", info.iconKey || "")
    }
    // Review snapshots (Main.qml's "libgroup=<name>", "libfocus=<n>"): a filter chip, a card's keyboard focus.
    function review(key, value) {
        if (key === "libgroup")
            root.groupFilter = chips.groupPrefix + (value === "-" ? "" : value)
        else if (key === "libfocus")
            keys.focusCard(keys.sectionList()[0], parseInt(value))
    }
    // A group's ⋯ menu, for review snapshots (Main.qml's "groupmenu=<name>").
    function openGroupMenu(group) {
        if (!root.grouped)
            chips.openMenu(chips.groupPrefix + group, chips)
        for (var i = 0; i < sections.count; ++i) {
            var section = sections.itemAt(i)
            if (section && section.group === group && section.manageable)
                section.openGroupMenu()
        }
    }
    // The instance ⋯ menu for @p id under @p anchor; `name`/`iconKey` are used when there is no
    // `library` to describe the instance. Without an anchor it opens at the top-left of the cards.
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
                // Ungrouped instances get no header: a named "Ungrouped" group read as one more
                // user group.
                showHeader: modelData.length > 0
                group: modelData
                model: root.sectionModelFor(modelData)
                columns: root.columns
                cardWidth: root.cardWidth
                gutter: root.gutter
                selectedId: root.selectedId
                viewMode: root.viewMode
                collapsed: !root.searching && modelData.length > 0 && root.collapsedGroups.indexOf(modelData) >= 0
                onToggleRequested: root.setCollapsed(modelData, root.collapsedGroups.indexOf(modelData) < 0)
                manageable: !!root.library && modelData.length > 0
                dragEnabled: !!root.library && root.groups.length > 1
                navigator: keys.navigate
                onRenameGroupRequested: (group) => groupDialogs.askRename(group)
                onDeleteGroupRequested: (group) => groupDialogs.askDelete(group)
                onInstanceDropped: (id, group) => root.library.moveToGroup(id, group)
                onDragActiveChanged: (active) => root.dragging = active
                onDragMoved: (sceneY) => root.dragSceneY = sceneY
                onSelectRequested: (id) => root.selectRequested(id)
                onOpenRequested: (id) => root.editRequested(id)
                onLaunchRequested: (id) => root.launchRequested(id)
                onStopRequested: (id) => root.stopRequested(id)
                onMenuRequested: (id, running, name, iconKey, group, anchor) => instanceMenu.openFor(id, running, name, iconKey, group, anchor)
            }
        }

        // One grid of every instance, the groups a filter over it.
        Column {
            visible: !root.grouped
            width: sectionsColumn.width
            spacing: Theme.space.lg

            LibraryGroupChips {
                id: chips
                width: parent.width
                // Not `visible`: the chips decide that themselves.
                instanceModel: root.grouped ? null : root.instanceModel
                sectionModelFor: root.sectionModelFor
                current: root.groupFilter
                manageable: !!root.library
                onActivated: (key) => root.groupFilter = key
                onRenameRequested: (group) => groupDialogs.askRename(group)
                onDeleteRequested: (group) => groupDialogs.askDelete(group)
            }

            InstanceSection {
                id: allSection
                width: parent.width
                showHeader: false
                model: root.grouped ? null
                     : root.filtered ? root.sectionModelFor(chips.groupOf(root.groupFilter)) : root.instanceModel
                navigator: keys.navigate
                columns: root.columns
                cardWidth: root.cardWidth
                gutter: root.gutter
                selectedId: root.selectedId
                viewMode: root.viewMode
                onSelectRequested: (id) => root.selectRequested(id)
                onOpenRequested: (id) => root.editRequested(id)
                onLaunchRequested: (id) => root.launchRequested(id)
                onStopRequested: (id) => root.stopRequested(id)
                // This section has no group of its own: the menu is told each instance's.
                onMenuRequested: (id, running, name, iconKey, group, anchor) =>
                    instanceMenu.openFor(id, running, name, iconKey,
                                         root.library ? (root.library.describe(id).group || "") : "", anchor)
            }
        }
    }

    LibraryKeys {
        id: keys
        flickable: root.flickable
        columns: root.viewMode === "list" ? 1 : root.columns
        sectionList: () => {
            var list = []
            for (var i = 0; root.grouped && i < sections.count; ++i) {
                if (!sections.itemAt(i).collapsed)
                    list.push(sections.itemAt(i))
            }
            return root.grouped ? list : [allSection]
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
        onPerformanceChosen: (id, enabled) => root.performanceChosen(id, enabled)
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    InstanceExportFlows {
        id: flows
        exportControllerFor: root.exportControllerFor
        shareCodes: root.shareCodes
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

    GroupDialogs {
        id: groupDialogs
        library: root.library
        // The renamed group stays the one shown.
        onRenamed: (from, to) => {
            if (root.groupFilter === chips.groupPrefix + from && root.groups.indexOf(to) >= 0)
                root.groupFilter = chips.groupPrefix + to
        }
    }
}
