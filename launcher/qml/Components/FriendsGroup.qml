// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One group of the Friends tab ("PLAYING — 2"): a caption over the friends
 * who are in it, each with Join when they can be joined and a ⋯ to remove
 * or block them (the page's FriendPersonMenu, `menu`). It hides itself
 * while the group is empty. It only says what was asked for; the page
 * confirms and acts.
 */
Column {
    id: root

    // The caption, in capitals, with its count.
    property string title
    // Friend maps (FriendsController.friends).
    property var friends: []
    // The page's FriendPersonMenu.
    property var menu: null
    // Main.qml's FriendJoinFlow, which says what Join would start.
    property var joinFlow: null
    // The list is the last one read, not a live one.
    property bool stale: false

    signal joinRequested(var friend)

    visible: root.friends.length > 0
    spacing: Theme.space.xxs

    FriendsSectionHeader {
        width: parent.width
        text: root.title
    }

    Repeater {
        model: root.friends
        delegate: FriendStatusRow {
            id: row
            required property var modelData

            width: root.width
            friend: modelData
            stale: root.stale

            Button {
                visible: Format.friendJoinable(row.modelData)
                buttonSize: "sm"
                text: qsTr("Join")
                // The play glyph every control that starts the game has.
                icon.source: Icons.url("play")
                Accessible.name: qsTr("Join %1 on %2").arg(row.modelData.name).arg(row.modelData.host)
                // What it does: it starts the game, with the instance named.
                // Worked out as the pointer arrives, so it is never an old answer.
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: hovered && root.joinFlow ? root.joinFlow.tip(row.modelData) : ""
                onClicked: root.joinRequested(row.modelData)
            }
            FriendMoreButton {
                menu: root.menu
                personUuid: row.modelData.uuid
                personName: row.modelData.name
            }
        }
    }
}
