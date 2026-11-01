// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The body of a Friends tab: whatever is declared inside it stacks in a column that scrolls, with a scroll bar and
 * ScrollFade's fade. The three tabs share it. The rows can be switched off (`actionsEnabled`) while the list stays
 * scrollable: disabling the tab would take the scrolling with it. With no rows (`count`) once the lists are read,
 * `emptyState` shows instead of the list.
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
    // Rows in the tab.
    property int count: 0

    // Not before the lists have been read: an empty list that is still arriving is not "no one".
    readonly property bool empty: !!root.controller && root.controller.loaded && root.count === 0

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
            }
        }

        ScrollFade { flickable: flick }
    }

    Item {
        id: emptySlot
        anchors.fill: parent
        visible: root.empty
    }
}
