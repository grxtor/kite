// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A FriendRow for one of your friends (a friend map, FriendsController.
 * friends): their head, name and where they are. Whatever goes inside it
 * (Join, ⋯) lands on its right.
 *
 * A friend on a visible server who does not let others join has no Join,
 * which would look like a bug: the row says so. On the Friends page a faint
 * "Joining off" shows in Join's place while the pointer is on the row, with
 * the reason as its tooltip; in the compact Home panel the row's own
 * tooltip says it.
 *
 * `stale`: the list is the last one read and may be out of date (the server
 * is not answering), so nobody is shown as around right now: no presence
 * dot, though an offline head stays dimmed.
 */
FriendRow {
    id: root

    property var friend: ({})
    property bool stale: false

    readonly property var status: Format.friendStatus(root.friend)
    readonly property bool joinOff: Format.friendJoinOff(root.friend)
    readonly property string joinOffTip: qsTr("%1 doesn't let friends join").arg(root.friend.name)
    readonly property int tipDelayMs: 500

    face: root.friend.face
    name: root.friend.name
    presence: root.stale && root.friend.state !== "offline" ? "" : root.friend.state
    detail: root.status.text
    detailSuffix: root.status.suffix
    detailTone: root.status.tone

    ToolTip.visible: root.compact && root.joinOff && root.hovered
    ToolTip.delay: root.tipDelayMs
    ToolTip.text: root.joinOffTip

    // Its room is kept, so the name does not shorten as it appears.
    Text {
        visible: !root.compact && root.joinOff
        // A Row puts a short child at its top; the buttons beside it are
        // centred, so the label is too.
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Joining off")
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        opacity: root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        HoverHandler { id: joinOffHover }
        ToolTip.visible: joinOffHover.hovered
        ToolTip.delay: root.tipDelayMs
        ToolTip.text: root.joinOffTip
    }
}
