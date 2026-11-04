// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One friend in the friends strip: only their head (FriendHead) on a square with the lists' hover fill, and on its
 * top corner what they want from you: an envelope for an invite, else the count of unread messages. The name and
 * where they are come as a tip at its left (above or below would cover the next head); a click opens the friend's
 * card (FriendHeadCard). `stale`: the list is the last one read, so no presence dot.
 */
AbstractButton {
    id: root

    property var friend: ({})
    property bool stale: false
    property bool selected: false
    // FriendPlaces.of(friend), or ({}): the tip names the server.
    property var place: ({})
    property bool invited: false
    readonly property int unread: root.friend.unread || 0
    readonly property string statusLine: Format.friendStatusLine(root.friend, root.place)

    readonly property int headSize: 40
    readonly property int badgeSize: 20
    readonly property int badgeGlyphSize: 12
    readonly property int tipDelayMs: 500
    readonly property color fill: root.hovered || root.selected || root.visualFocus
                                  ? Theme.palette.surfaceHover : Theme.palette.canvas
    property bool tipReady: false

    implicitWidth: Theme.control.heightLg
    implicitHeight: Theme.control.heightLg
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: qsTr("%1, %2").arg(root.friend.name).arg(root.statusLine)
    Accessible.description: root.invited ? qsTr("Invited you") : Format.unreadMessages(root.unread)

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
            id: head
            anchors.centerIn: parent
            size: root.headSize
            face: root.friend.face
            presence: root.stale && root.friend.state !== "offline" ? "" : root.friend.state
            ringColor: root.fill
        }
        CountPill {
            x: head.x + head.width - width + Theme.space.xs
            y: head.y - Theme.space.xs
            count: root.invited ? 0 : root.unread
            ringColor: root.fill
        }
        // An invite: an envelope in the accent, ringed like the pill.
        Rectangle {
            visible: root.invited
            x: head.x + head.width - width + Theme.space.xs
            y: head.y - Theme.space.xs
            width: root.badgeSize + Theme.focus.width * 2
            height: width
            radius: width / 2
            color: root.fill

            Rectangle {
                anchors.centerIn: parent
                width: root.badgeSize
                height: width
                radius: width / 2
                color: Theme.palette.accent

                MeshIcon {
                    anchors.centerIn: parent
                    iconName: "mail"
                    size: root.badgeGlyphSize
                    color: Theme.palette.textOnAccent
                }
            }
        }
    }

    ToolTip {
        visible: root.tipReady && !root.selected
        x: -width - Theme.space.sm
        y: (root.height - height) / 2
        text: root.friend.name + "\n" + root.statusLine
    }
}
