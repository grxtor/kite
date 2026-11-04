// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A FriendRow for one of your friends (a friend map). On a server its line is the server's name and favicon when
 * `place` (FriendPlaces.of()) knows them. Unread messages are counted before the buttons. On a visible server where
 * the friend does not let others join there is no Join: the Friends page shows a faint "Joining off" on row hover,
 * the compact Home panel a tooltip. `stale`: the list is the last one read, so no presence dot.
 */
FriendRow {
    id: root

    property var friend: ({})
    property bool stale: false
    // FriendPlaces.of(friend), or ({}).
    property var place: ({})

    readonly property var status: Format.friendStatus(root.friend, root.place)
    readonly property bool joinOff: Format.friendJoinOff(root.friend)
    readonly property string joinOffTip: qsTr("%1 doesn't let friends join").arg(root.friend.name)
    // Chat messages from them you have not read; 0 until the launcher has chat.
    readonly property int unread: root.friend.unread || 0
    readonly property int tipDelayMs: 500

    face: root.friend.face
    name: root.friend.name
    presence: root.stale && root.friend.state !== "offline" ? "" : root.friend.state
    detail: root.status.text
    detailSuffix: root.status.suffix
    detailTone: root.status.tone
    detailServer: !!root.place.address
    detailIcon: root.place.favicon || ""
    Accessible.description: Format.unreadMessages(root.unread)

    ToolTip.visible: root.compact && root.joinOff && root.hovered
    ToolTip.delay: root.tipDelayMs
    ToolTip.text: root.joinOffTip

    CountPill {
        anchors.verticalCenter: parent.verticalCenter
        count: root.unread
        ringColor: root.fill
    }
    Copy {
        visible: !root.compact && root.joinOff
        // A Row puts a short child at its top; the buttons beside it are centred.
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Joining off")
        role: "label"
        tone: "tertiary"
        opacity: root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        HoverHandler { id: joinOffHover }
        ToolTip.visible: joinOffHover.hovered
        ToolTip.delay: root.tipDelayMs
        ToolTip.text: root.joinOffTip
    }
}
