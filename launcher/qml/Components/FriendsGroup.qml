// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One group of the Friends tab ("In game · 2"): a caption over a card of the friends in it (FriendsListCard), each
 * saying where they play (server favicon and name, game version), with Join when joinable, Message, and a ⋯ to
 * invite, remove or block (the page's FriendPersonMenu, `menu`; a right-click opens it too). A click on the row
 * is Message. Hides itself while empty; it only says what was asked for, the page confirms and acts.
 */
Column {
    id: root

    property string title
    property var friends: []
    property var menu: null
    property var joinFlow: null
    property bool stale: false
    // Main.qml's FriendPlaces, for the server names and favicons.
    property FriendPlaces places: FriendPlaces {}

    signal joinRequested(var friend)
    signal messageRequested(string uuid)

    visible: root.friends.length > 0

    FriendsSectionHeader {
        width: parent.width
        text: root.title
    }

    FriendsListCard {
        id: card
        width: parent.width

        ListEntrance { id: entrance; view: rows; watchModel: false }
        Repeater {
            id: rows
            model: root.friends
            delegate: FriendStatusRow {
                id: row
                required property var modelData
                required property int index

                width: card.rowWidth
                surface: card.color
                divider: row.index > 0
                friend: modelData
                stale: root.stale
                place: root.places.of(modelData)
                interactive: true
                onClicked: root.messageRequested(row.modelData.uuid)
                onMenuRequested: more.clicked()
                RiseIn { target: row; index: row.index; gate: entrance }

                FriendJoinButton {
                    friend: row.modelData
                    joinFlow: root.joinFlow
                    icon.source: Icons.url("play")
                    onClicked: root.joinRequested(row.modelData)
                }
                IconButton {
                    size: Theme.control.heightSm
                    iconName: "message-circle"
                    tip: qsTr("Message")
                    Accessible.name: qsTr("Message %1").arg(row.modelData.name)
                    onClicked: root.messageRequested(row.modelData.uuid)
                }
                FriendMoreButton {
                    id: more
                    menu: root.menu
                    personUuid: row.modelData.uuid
                    personName: row.modelData.name
                }
            }
        }
    }
}
