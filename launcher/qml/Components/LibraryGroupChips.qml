// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The library's groups as a filter over its one grid: "All", a chip per group and "Ungrouped", each with how many
 * instances it holds. `current` is "all" or "g:<group>" ("g:" for the ungrouped ones). A group's chip opens the
 * group's menu on a right click or the Menu key, and the ⋯ after the chips does for the selected group. Chips that
 * do not fit scroll sideways. Hidden while no instance is in a group.
 */
Item {
    id: root

    // The library proxy (`count`, `groups`) and group -> that group's model, for the counts.
    property var instanceModel: null
    property var sectionModelFor: null
    property string current: "all"
    // Groups can be renamed and deleted (there is a LibraryController).
    property bool manageable: false

    signal activated(string key)
    signal renameRequested(string group)
    signal deleteRequested(string group)

    readonly property string allKey: "all"
    readonly property string groupPrefix: "g:"
    function groupOf(key) {
        return key.substring(root.groupPrefix.length)
    }

    // Sorted here: without grouping the proxy lists its groups in row order.
    readonly property var named: (root.instanceModel && root.instanceModel.groups ? root.instanceModel.groups : [])
        .filter((group) => group.length > 0).sort((a, b) => a.localeCompare(b))
    readonly property bool hasUngrouped: !!root.instanceModel && !!root.instanceModel.groups
                                         && root.instanceModel.groups.indexOf("") >= 0
    readonly property bool currentIsGroup: root.current !== root.allKey && root.current.length > root.groupPrefix.length
    readonly property var options: {
        var counted = (group) => {
            var model = root.sectionModelFor ? root.sectionModelFor(group) : null
            return model ? model.count : 0
        }
        var list = [{ value: root.allKey, label: qsTr("All"), count: root.instanceModel ? root.instanceModel.count : 0 }]
        for (var group of root.named)
            list.push({ value: root.groupPrefix + group, label: group, count: counted(group) })
        if (root.hasUngrouped)
            list.push({ value: root.groupPrefix, label: qsTr("Ungrouped"), count: counted("") })
        return list
    }

    function openMenu(key, anchor) {
        if (!root.manageable || key.length <= root.groupPrefix.length || key === root.allKey)
            return
        menu.group = root.groupOf(key)
        menu.popup(anchor, 0, anchor.height + Theme.space.xs)
    }

    visible: root.named.length > 0
    implicitHeight: visible ? Theme.control.heightSm : 0

    Flickable {
        id: scroller
        width: Math.min(chips.width, root.width - (moreButton.visible ? moreButton.width + Theme.space.xs : 0))
        height: root.height
        contentWidth: chips.width
        contentHeight: height
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
        interactive: contentWidth > width
        clip: interactive

        ChipGroup {
            id: chips
            options: root.options
            current: root.current
            onActivated: (value) => root.activated(value)
            onContextRequested: (value, chip) => root.openMenu(value, chip)
        }
    }

    IconButton {
        id: moreButton
        visible: root.manageable && root.currentIsGroup
        x: scroller.width + Theme.space.xs
        anchors.verticalCenter: parent.verticalCenter
        size: Theme.control.heightSm
        iconName: "more"
        tip: qsTr("Group options")
        onClicked: root.openMenu(root.current, moreButton)
    }

    GroupMenu {
        id: menu
        onRenameRequested: (group) => root.renameRequested(group)
        onDeleteRequested: (group) => root.deleteRequested(group)
    }
}
