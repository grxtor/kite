// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The top bar's button for the friends panel: the users glyph, lit while the panel is out, with how many friends are
 * online on its corner. Main.qml keeps the choice (the FriendsPanelOpen setting). While the panel is hidden, waiting
 * requests take the other corner.
 */
IconButton {
    id: root

    property bool open: false
    property int onlineCount: 0
    property int requestCount: 0
    property color surround: Theme.palette.canvas

    size: Theme.control.heightSm
    iconName: "users"
    tip: root.open ? qsTr("Hide friends") : qsTr("Show friends")
    // Mirrors `open` for the look and for accessibility; the button must not
    // toggle it itself, which would drop the binding.
    checked: root.open
    Accessible.description: root.onlineCount > 0 ? qsTr("%1 online").arg(root.onlineCount) : ""

    // One pill at a time on the lower corner, so the glyph stays readable: waiting requests while the panel is
    // hidden (they need an answer), otherwise how many friends are online.
    readonly property bool showsRequests: !root.open && root.requestCount > 0

    CountPill {
        x: root.width - width + Theme.space.xs
        y: root.height - height + Theme.space.xs
        visible: root.showsRequests ? root.requestCount > 0 : root.onlineCount > 0
        count: root.showsRequests ? root.requestCount : root.onlineCount
        tone: root.showsRequests ? "accent" : "success"
        ringColor: root.surround
    }
}
