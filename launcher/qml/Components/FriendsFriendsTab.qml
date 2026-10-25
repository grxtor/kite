// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Friends page's "Friends" tab: everyone on the list in three groups --
 * playing, online, offline -- each a caption over rows that sit on the page
 * (FriendsGroup). With nobody on the list it is FriendsEmptyState instead;
 * until the list has been read it is neither, so a person with friends is
 * not told they have none while it loads.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // The page's FriendPersonMenu.
    property var menu: null
    // Main.qml's FriendJoinFlow.
    property var joinFlow: null
    // The list is the last one read, not a live one.
    property bool stale: false
    // The server answers: Join and ⋯ need it, reading the list does not.
    property bool actionsEnabled: true

    signal joinRequested(var friend)

    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])
    readonly property bool hasFriends: !!root.controller && root.controller.friends.length > 0
    readonly property bool empty: !!root.controller && root.controller.loaded && !root.hasFriends

    FriendsListScroll {
        anchors.fill: parent
        visible: root.hasFriends
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.sm

        FriendsGroup {
            width: parent.width
            title: qsTr("PLAYING — %1").arg(root.groups.playing.length)
            friends: root.groups.playing
            menu: root.menu
            joinFlow: root.joinFlow
            stale: root.stale
            onJoinRequested: (friend) => root.joinRequested(friend)
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("ONLINE — %1").arg(root.groups.online.length)
            friends: root.groups.online
            menu: root.menu
            stale: root.stale
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("OFFLINE — %1").arg(root.groups.offline.length)
            friends: root.groups.offline
            menu: root.menu
            stale: root.stale
        }
    }

    FriendsEmptyState {
        anchors.fill: parent
        visible: root.empty
        myName: root.controller ? root.controller.myName : ""
    }
}
