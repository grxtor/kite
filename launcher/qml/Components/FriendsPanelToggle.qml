// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The top bar's button for the friends panel on the right edge: the users
 * glyph, lit (the toolbar toggles' checked look) while the panel is out, and
 * on its corner how many friends are online. One click shows or hides the
 * panel; Main.qml keeps the choice (the FriendsPanelOpen setting). While the
 * panel is hidden, waiting requests take the other corner: the header's bell
 * that shows them is hidden with it.
 */
IconButton {
    id: root

    // Whether the panel is out.
    property bool open: false
    // Friends who are not offline; 0 while that is not known.
    property int onlineCount: 0
    // Friend requests waiting for an answer.
    property int requestCount: 0
    // The colour the top bar is drawn on, for the count's ring.
    property color surround: Theme.palette.canvas

    size: Theme.control.heightSm
    iconName: "users"
    tip: root.open ? qsTr("Hide friends") : qsTr("Show friends")
    // Mirrors `open` for the look and for accessibility; the button must not
    // toggle it itself, which would drop the binding.
    checked: root.open
    Accessible.description: root.onlineCount > 0 ? qsTr("%1 online").arg(root.onlineCount) : ""

    // On the corner, a little out of it, so it covers less of the glyph.
    CountPill {
        x: root.width - width + Theme.space.xs
        y: root.height - height + Theme.space.xs
        count: root.onlineCount
        tone: "success"
        ringColor: root.surround
    }

    CountPill {
        x: root.width - width + Theme.space.xs
        y: -Theme.space.xs
        visible: !root.open && root.requestCount > 0
        count: root.requestCount
        tone: "accent"
        ringColor: root.surround
    }
}
