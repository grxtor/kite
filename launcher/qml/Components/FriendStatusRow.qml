// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A FriendRow for one of your friends (a friend map). On a visible server where the friend does not let others
 * join there is no Join: the Friends page shows a faint "Joining off" on row hover, the compact Home panel a
 * tooltip. `stale`: the list is the last one read, so no presence dot.
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

    Text {
        visible: !root.compact && root.joinOff
        // A Row puts a short child at its top; the buttons beside it are centred.
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
