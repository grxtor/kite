// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Friends page's "Friends" tab: everyone on the list in three groups -- in game, in Kite, offline
 * (FriendsGroup).
 * With nobody on the list it is FriendsEmptyState; until the list has been read it is neither, so a person with
 * friends is not told they have none while it loads.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    property var menu: null
    property var joinFlow: null
    property bool stale: false
    property bool actionsEnabled: true
    // Main.qml's FriendPlaces, for the server names and favicons.
    property FriendPlaces places: FriendPlaces {}

    signal joinRequested(var friend)

    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])

    FriendsListScroll {
        anchors.fill: parent
        controller: root.controller
        count: root.controller ? root.controller.friends.length : 0
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.sm
        emptyState: FriendsEmptyState { myName: root.controller ? root.controller.myName : "" }

        FriendsGroup {
            width: parent.width
            title: qsTr("IN GAME — %1").arg(root.groups.playing.length)
            places: root.places
            friends: root.groups.playing
            menu: root.menu
            joinFlow: root.joinFlow
            stale: root.stale
            onJoinRequested: (friend) => root.joinRequested(friend)
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("IN KITE — %1").arg(root.groups.online.length)
            places: root.places
            friends: root.groups.online
            menu: root.menu
            stale: root.stale
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("OFFLINE — %1").arg(root.groups.offline.length)
            places: root.places
            friends: root.groups.offline
            menu: root.menu
            stale: root.stale
        }
    }
}
