// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One friend in the friends strip: only their head (FriendHead, with the
 * presence dot) on a square that takes the lists' hover fill. The name and
 * where they are come as a tip at its left -- above or below would cover the
 * next head -- and a click opens the friend's card (FriendHeadCard), which
 * is where Join and ⋯ are.
 *
 * `stale`: the list is the last one read and may be out of date, so nobody
 * is shown as around right now: no presence dot, though an offline head
 * stays dimmed (as FriendStatusRow does).
 */
AbstractButton {
    id: root

    // A friend map (FriendsController.friends).
    property var friend: ({})
    property bool stale: false
    // This friend's card is the one open.
    property bool selected: false

    // A head is pixel art: a multiple of 8 (see FriendHead).
    readonly property int headSize: 40
    readonly property int tipDelayMs: 500
    // The fill the head's ring has to match.
    readonly property color fill: root.hovered || root.selected || root.visualFocus
                                  ? Theme.palette.surfaceHover : Theme.palette.canvas
    // The pointer has rested on the head long enough for the tip.
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

    background: Rectangle {
        radius: Theme.radius.md
        color: root.hovered || root.selected || root.visualFocus ? Theme.palette.surfaceHover : "transparent"
        border.width: root.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
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
        // The card says it all once it is open.
        visible: root.tipReady && !root.selected
        x: -width - Theme.space.sm
        y: (root.height - height) / 2
        text: root.friend.name + "\n" + Format.friendStatusLine(root.friend)
    }
}
