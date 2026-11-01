// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One friend in the friends strip: only their head (FriendHead) on a square with the lists' hover fill. The name
 * and where they are come as a tip at its left (above or below would cover the next head); a click opens the
 * friend's card (FriendHeadCard). `stale`: the list is the last one read, so no presence dot.
 */
AbstractButton {
    id: root

    property var friend: ({})
    property bool stale: false
    property bool selected: false

    readonly property int headSize: 40
    readonly property int tipDelayMs: 500
    readonly property color fill: root.hovered || root.selected || root.visualFocus
                                  ? Theme.palette.surfaceHover : Theme.palette.canvas
    property bool tipReady: false

    implicitWidth: Theme.control.heightLg
    implicitHeight: Theme.control.heightLg
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: qsTr("%1, %2").arg(root.friend.name).arg(Format.friendStatusLine(root.friend))

    onHoveredChanged: if (!root.hovered) root.tipReady = false
    Timer {
        interval: root.tipDelayMs
        running: root.hovered
        onTriggered: root.tipReady = true
    }

    background: StateFill {
        card: true
        rest: "transparent"
        hovered: root.hovered || root.visualFocus
        active: root.selected
        radius: Theme.radius.md

        FocusFrame { around: 0; visible: root.visualFocus }
    }

    contentItem: Item {
        FriendHead {
            anchors.centerIn: parent
            size: root.headSize
            face: root.friend.face
            presence: root.stale && root.friend.state !== "offline" ? "" : root.friend.state
            ringColor: root.fill
        }
    }

    ToolTip {
        visible: root.tipReady && !root.selected
        x: -width - Theme.space.sm
        y: (root.height - height) / 2
        text: root.friend.name + "\n" + Format.friendStatusLine(root.friend)
    }
}
