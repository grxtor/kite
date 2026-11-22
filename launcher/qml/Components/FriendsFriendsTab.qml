// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Friends page's "Friends" tab: invites to a server first (FriendsWaiting), then everyone on the list in three
 * groups -- in game, in Kite, offline (FriendsGroup).
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
    // FriendsPage's FriendsState, for the invites.
    property FriendsState friendsState: FriendsState { controller: root.controller }
    // Main.qml's FriendPlaces, for the server names and favicons.
    property FriendPlaces places: FriendPlaces {}

    // The page's search: only friends whose name has it show.
    property string query

    signal joinRequested(var friend)
    signal messageRequested(string uuid)
    signal addRequested()
    signal clearRequested()

    readonly property var all: root.controller ? root.controller.friends : []
    readonly property var shown: root.friendsState.named(root.all, root.query)
    readonly property var groups: Format.friendsGrouped(root.shown)

    function enter() { list.enter() }

    FriendsListScroll {
        id: list
        anchors.fill: parent
        controller: root.controller
        count: root.shown.length
        total: root.all.length
        query: root.query
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.sm
        onClearRequested: root.clearRequested()
        emptyState: FriendsEmptyState {
            myName: root.controller ? root.controller.myName : ""
            onActionTriggered: root.addRequested()
        }

        FriendsWaiting {
            width: parent.width
            visible: root.query.length === 0 && invites.length > 0
            friendsState: root.friendsState
            places: root.places
            joinFlow: root.joinFlow
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("In game · %1").arg(root.groups.playing.length)
            places: root.places
            friends: root.groups.playing
            menu: root.menu
            joinFlow: root.joinFlow
            stale: root.stale
            onJoinRequested: (friend) => root.joinRequested(friend)
            onMessageRequested: (uuid) => root.messageRequested(uuid)
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("In Kite · %1").arg(root.groups.online.length)
            places: root.places
            friends: root.groups.online
            menu: root.menu
            stale: root.stale
            onMessageRequested: (uuid) => root.messageRequested(uuid)
        }
        FriendsGroup {
            width: parent.width
            title: qsTr("Offline · %1").arg(root.groups.offline.length)
            places: root.places
            friends: root.groups.offline
            menu: root.menu
            stale: root.stale
            onMessageRequested: (uuid) => root.messageRequested(uuid)
        }
    }
}
