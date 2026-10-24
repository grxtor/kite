// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The instance library: every group as its own section. The instance most
 * likely to play next no longer gets its own hero card here -- the
 * persistent play bar (PlayDock, in Main.qml) replaces it, showing on every
 * page rather than just this one.
 *
 * The sections, the ⋯ menus and their dialogs are LibrarySections, which
 * HomePage shows too, under "Continue playing"; this page is that and its
 * own scrolling, empty state and page frame.
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
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    // id -> export / shortcut controller (QmlShell, set in Main.qml). The ⋯
    // menu offers "Export…" and "Create shortcut…" once these are set; the
    // dialogs themselves are LibrarySections' InstanceExportFlows.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    // LibraryController (QmlShell's `library`, set in Main.qml): group
    // management, folds that persist, dragging an instance onto a group,
    // the instance's folders and Duplicate with options. Until it is set
    // the page keeps its session-only folds and Main.qml's duplicate.
    property var library: null

    function exportInstance(id, format) {
        sections.exportInstance(id, format)
    }
    function createShortcut(id) {
        sections.createShortcut(id)
    }
    // For review snapshots (Main.qml's "groupmenu=<name>" and
    // "duplicate=<id>" dev routes): a group's ⋯ menu, and the duplicate
    // dialog.
    function openGroupMenu(group) {
        sections.openGroupMenu(group)
    }
    function duplicateInstance(id) {
        sections.duplicateInstance(id)
    }
    // The card ⋯ menu for instance @p id, for review snapshots (Main.qml's
    // "librarymenu=<id>" dev route), at the top-left of the cards.
    function openInstanceMenu(id) {
        sections.openInstanceMenu(id, false, null, "", "")
    }

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int emptyStateWidth: 400
    // On a wide window the content stops at Theme.layout.maxContentWidth
    // and centres (TopBar uses the same rule, so its title and toolbar stay
    // over the cards' left edge).
    readonly property int contentWidth: Math.max(0, Math.min(flick.width - pagePadding * 2, Theme.layout.maxContentWidth))
    readonly property int contentX: Math.max(pagePadding, Math.round((flick.width - contentWidth) / 2))

    readonly property bool searching: searchText.length > 0
    readonly property int instanceCount: instanceModel && instanceModel.count !== undefined ? instanceModel.count : 0

    Keys.onEscapePressed: root.selectRequested("")

    // No page-wide photo wash behind the grid: the cards' own covers are
    // the artwork, and the page stays the flat canvas the top bar sits on,
    // so there is no edge where the two meet.

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: sections.y + sections.height + root.pagePadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: !sections.dragging

        ScrollBar.vertical: ScrollBar {}

        // Clicking the empty page clears the selection.
        MouseArea {
            width: flick.width
            height: Math.max(flick.height, flick.contentHeight)
            onClicked: root.selectRequested("")
        }

        // Every group, the ⋯ menus and their dialogs (shared with Home).
        LibrarySections {
            id: sections
            x: root.contentX
            // Clear of the toolbar above the page (Main.qml); the shell's
            // panel adds no padding of its own.
            y: Theme.space.lg
            width: root.contentWidth
            height: implicitHeight
            flickable: flick
            instanceModel: root.instanceModel
            sectionModelFor: root.sectionModelFor
            searchText: root.searchText
            selectedId: root.selectedId
            viewMode: root.viewMode
            launchOptionsFor: root.launchOptionsFor
            exportControllerFor: root.exportControllerFor
            shortcutControllerFor: root.shortcutControllerFor
            library: root.library

            onSelectRequested: (id) => root.selectRequested(id)
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
        // A readable measure for the body text; EmptyState caps it.
        width: Math.min(root.width - root.pagePadding * 2, root.emptyStateWidth)
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
}
