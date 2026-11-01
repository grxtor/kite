// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One group of the Friends tab ("PLAYING — 2"): a caption over the friends in it, each with Join when joinable and a
 * ⋯ to remove or block (the page's FriendPersonMenu, `menu`). Hides itself while empty; it only says what was asked
 * for, the page confirms and acts.
 */
Column {
    id: root

    property string title
    property var friends: []
    property var menu: null
    property var joinFlow: null
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

            FriendJoinButton {
                friend: row.modelData
                joinFlow: root.joinFlow
                icon.source: Icons.url("play")
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
