// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Join on one of your friends: shown only when `friend` can be joined, named for a screen reader, with the same
 * words as FriendJoinFlow.tip() as its tooltip. Whoever uses it handles clicked(). `iconOnly`: a square with just the
 * play glyph, for a row with no room for the word.
 */
Button {
    id: root

    property var friend: ({})
    // The Main.qml FriendJoinFlow, or null.
    property var joinFlow: null
    property bool iconOnly: false

    visible: Format.friendJoinable(root.friend)
    buttonSize: "sm"
    text: qsTr("Join")
    display: root.iconOnly ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
    icon.source: root.iconOnly ? Icons.url("play") : ""
    icon.width: root.iconOnly ? Theme.icon.mid : root.sizeIcon
    icon.height: root.iconOnly ? Theme.icon.mid : root.sizeIcon
    leftPadding: root.iconOnly ? (Theme.control.heightSm - Theme.icon.mid) / 2 : root.horizontalPadding
    rightPadding: leftPadding
    Accessible.name: qsTr("Join %1 on %2").arg(root.friend.name).arg(root.friend.host)
    // Worked out as the pointer arrives, so it is never an old answer.
    ToolTip.visible: root.hovered
    ToolTip.delay: 500
    ToolTip.text: root.hovered && root.joinFlow ? root.joinFlow.tip(root.friend) : ""
}
