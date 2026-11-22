// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The body of a Friends tab: whatever is declared inside it stacks in a column that scrolls, with a scroll bar and
 * ScrollFade's fade. The three tabs share it. The rows can be switched off (`actionsEnabled`) while the list stays
 * scrollable: disabling the tab would take the scrolling with it. With no rows (`count`) once the lists are read,
 * `emptyState` shows instead of the list; with rows but none that match the search (`total`, `query`), a line
 * says so and offers to clear it.
 */
Item {
    id: root

    default property alias content: column.data
    // The tab's empty state (an EmptyState); it fills this item.
    property alias emptyState: emptySlot.data
    property alias spacing: column.spacing
    property bool actionsEnabled: true
    // FriendsController, or null.
    property var controller: null
    // Rows in the tab, and how many there are before the search (`query`) narrowed them.
    property int count: 0
    property int total: root.count
    property string query

    signal clearRequested()

    // Not before the lists have been read: an empty list that is still arriving is not "no one".
    readonly property bool empty: !!root.controller && root.controller.loaded && root.total === 0
    readonly property bool noMatch: root.total > 0 && root.count === 0

    // The tab was just switched to: its list comes in like a page, a fade and a rise that settles with the pop
    // easing.
    function enter() {
        if (!Theme.motion.reduced)
            entrance.restart()
    }
    ParallelAnimation {
        id: entrance
        NumberAnimation { target: column; property: "opacity"; from: 0; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        NumberAnimation { target: columnRise; property: "y"; from: Theme.motion.rise; to: 0; duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
    }

    Item {
        anchors.fill: parent
        visible: root.count > 0

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: width
            contentHeight: column.height + Theme.space.xl
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                width: flick.width
                enabled: root.actionsEnabled
                transform: Translate { id: columnRise }
            }
        }

        ScrollFade { flickable: flick }
    }

    EmptyState {
        anchors.fill: parent
        visible: root.noMatch
        upperThird: true
        iconName: "search"
        title: qsTr("No one called “%1”").arg(root.query)
        actionText: qsTr("Clear search")
        onActionTriggered: root.clearRequested()
    }

    Item {
        id: emptySlot
        anchors.fill: parent
        visible: root.empty
    }
}
