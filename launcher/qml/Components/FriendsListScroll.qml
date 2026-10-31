// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The scrolling body of a Friends tab: whatever is declared inside it stacks in a column that scrolls, with a scroll
 * bar and ScrollFade's fade. The three tabs share it. The rows can be switched off (`actionsEnabled`) while the list
 * stays scrollable: disabling the tab would take the scrolling with it.
 */
Item {
    id: root

    default property alias content: column.data
    property alias spacing: column.spacing
    property bool actionsEnabled: true

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

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }
}
