// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
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

            Button {
                visible: Format.friendJoinable(row.modelData)
                buttonSize: "sm"
                text: qsTr("Join")
                icon.source: Icons.url("play")
                Accessible.name: qsTr("Join %1 on %2").arg(row.modelData.name).arg(row.modelData.host)
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
