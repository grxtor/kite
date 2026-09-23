// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The instance library: a hero for the instance you are most likely to
 * play next, then every group as its own section. While searching, the hero
 * steps aside and only matching instances are listed.
 *
 * The page never builds models itself. It is handed the search-filtered
 * instance model, a recent-first model, a one-row hero model, and a
 * function that returns the model for one group -- all C++ proxies, owned
 * by the shell.
 */
Item {
    id: root

    property var instanceModel: null
    property var recentModel: null
    property var heroModel: null
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

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int gutter: Theme.space.lg
    readonly property int minCardWidth: 184
    readonly property int maxCardWidth: 240
    readonly property int contentWidth: Math.max(0, flick.width - pagePadding * 2)
    readonly property int columns: columnsFor(contentWidth)
    readonly property real cardWidth: Math.floor((contentWidth - gutter * (columns - 1)) / columns)

    readonly property bool searching: searchText.length > 0
    readonly property var groups: instanceModel && instanceModel.groups ? instanceModel.groups : []
    readonly property int instanceCount: instanceModel && instanceModel.count !== undefined ? instanceModel.count : 0

    readonly property string recentId: recentModel && recentModel.firstId ? recentModel.firstId : ""
    readonly property string heroId: selectedId.length > 0 ? selectedId
                                   : recentId.length > 0 ? recentId
                                   : instanceModel && instanceModel.firstId ? instanceModel.firstId : ""

    // Session-only: which groups the user folded away.
    property var collapsedGroups: ({})

    function columnsFor(width) {
        var columns = Math.max(1, Math.floor((width + gutter) / (minCardWidth + gutter)))
        while ((width - gutter * (columns - 1)) / columns > maxCardWidth)
            columns++
        return columns
    }

    Binding {
        target: root.heroModel
        property: "instanceId"
        value: root.heroId
        when: !!root.heroModel
    }

    Keys.onEscapePressed: root.selectRequested("")

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.y + content.height + root.pagePadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        // Clicking the empty page clears the selection.
        MouseArea {
            width: flick.width
            height: Math.max(flick.height, flick.contentHeight)
            onClicked: root.selectRequested("")
        }

        Column {
            id: content
            x: root.pagePadding
            y: Theme.space.xs
            width: root.contentWidth
            spacing: Theme.space.xl + Theme.space.sm

            Repeater {
                model: root.searching ? null : root.heroModel
                delegate: ContinueCard {
                    required property string group
                    width: content.width
                    overline: root.selectedId.length > 0 ? qsTr("Selected")
                            : instanceId === root.recentId ? qsTr("Continue playing")
                            : qsTr("Ready to play")
                    onPlayRequested: root.launchRequested(instanceId)
                    onStopRequested: root.stopRequested(instanceId)
                    onEditRequested: root.editRequested(instanceId)
                    onFolderRequested: root.folderRequested(instanceId)
                    onMenuRequested: instanceMenu.openFor(instanceId, isRunning, name, iconKey, group)
                }
            }

            Repeater {
                model: root.groups
                delegate: InstanceSection {
                    required property string modelData
                    width: content.width
                    title: modelData.length > 0 ? modelData : qsTr("Ungrouped")
                    // A library that never used groups gets no header at all.
                    showHeader: root.groups.length > 1 || modelData.length > 0
                    group: modelData
                    model: root.sectionModelFor(modelData)
                    columns: root.columns
                    cardWidth: root.cardWidth
                    gutter: root.gutter
                    selectedId: root.selectedId
                    viewMode: root.viewMode
                    collapsed: !root.searching && root.collapsedGroups[modelData] === true
                    onToggleRequested: {
                        var next = Object.assign({}, root.collapsedGroups)
                        next[modelData] = !(next[modelData] === true)
                        root.collapsedGroups = next
                    }
                    onSelectRequested: (id) => root.selectRequested(id)
                    onLaunchRequested: (id) => root.launchRequested(id)
                    onStopRequested: (id) => root.stopRequested(id)
                    onMenuRequested: (id, running, name, iconKey, group) => instanceMenu.openFor(id, running, name, iconKey, group)
                }
            }
        }
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

        function openFor(id, running, name, iconKey, group) {
            targetId = id
            targetRunning = running
            targetName = name || ""
            targetIcon = iconKey || ""
            targetGroup = group || ""
            popup()
        }

        MenuItem {
            text: instanceMenu.targetRunning ? qsTr("Stop") : qsTr("Play")
            icon.source: Icons.url(instanceMenu.targetRunning ? "stop" : "play")
            onTriggered: instanceMenu.targetRunning ? root.stopRequested(instanceMenu.targetId)
                                                    : root.launchRequested(instanceMenu.targetId)
        }
        MenuItem {
            text: qsTr("Open")
            icon.source: Icons.url("settings")
            onTriggered: root.editRequested(instanceMenu.targetId)
        }
        MenuItem {
            text: qsTr("Open folder")
            icon.source: Icons.url("folder")
            onTriggered: root.folderRequested(instanceMenu.targetId)
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
        MenuItem {
            text: qsTr("Move to group\u2026")
            icon.source: Icons.url("layers")
            onTriggered: root.groupRequested(instanceMenu.targetId, instanceMenu.targetGroup)
        }
        MenuItem {
            text: qsTr("Duplicate\u2026")
            icon.source: Icons.url("copy")
            onTriggered: root.duplicateRequested(instanceMenu.targetId, instanceMenu.targetName, instanceMenu.targetGroup)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete\u2026")
            icon.source: Icons.url("trash")
            enabled: !instanceMenu.targetRunning
            onTriggered: root.deleteRequested(instanceMenu.targetId, instanceMenu.targetName)
        }
    }
}
