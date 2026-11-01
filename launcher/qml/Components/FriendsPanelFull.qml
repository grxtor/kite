// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The friends panel opened out: header, then who is in game, online and, folded until opened, offline
 * (FriendsPanelGroup). Requests, blocked people and adding by name stay on the Friends page. FriendsSidePanel
 * builds this once and never tears it down, so the folds hold for the session.
 */
Item {
    id: root

    // FriendsSidePanel's FriendsState.
    property FriendsState friendsState: FriendsState {}
    property var joinFlow: null
    property var menu: null

    // Which groups show their rows; offline starts folded.
    property bool playingOpen: true
    property bool onlineOpen: true
    property bool offlineOpen: false

    // tab: "friends", "requests", "blocked" or "add".
    signal friendsRequested(string tab)
    signal navigateRequested(string target)
    signal joinRequested(var friend)
    signal collapseRequested()

    FriendsPanelHeader {
        id: header
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 1
        ready: root.friendsState.connected
        loaded: root.friendsState.loaded
        friendCount: root.friendsState.friendCount
        onlineCount: root.friendsState.onlineCount
        incomingCount: root.friendsState.incomingCount
        showsLists: root.friendsState.showsLists
        onAddRequested: root.friendsRequested("add")
        onRequestsRequested: root.friendsRequested("requests")
        onCollapseRequested: root.collapseRequested()
    }

    Item {
        id: lists
        visible: root.friendsState.showsLists
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: width
            contentHeight: column.y + column.height + Theme.space.xl
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                x: Theme.space.lg
                width: flick.width - Theme.space.lg * 2
                spacing: Theme.space.sm

                FriendsStaleBar {
                    width: parent.width
                    visible: root.friendsState.stale
                    controller: root.friendsState.controller
                }

                FriendsNotice {
                    width: parent.width
                    controller: root.friendsState.controller
                    onSettingsRequested: root.navigateRequested("settings/friends")
                }

                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("IN GAME — %1").arg(root.friendsState.groups.playing.length)
                    friends: root.friendsState.groups.playing
                    expanded: root.playingOpen
                    menu: root.menu
                    joinFlow: root.joinFlow
                    stale: root.friendsState.stale
                    actionsEnabled: root.friendsState.connected
                    onToggled: root.playingOpen = !root.playingOpen
                    onJoinRequested: (friend) => root.joinRequested(friend)
                }
                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("ONLINE — %1").arg(root.friendsState.groups.online.length)
                    friends: root.friendsState.groups.online
                    expanded: root.onlineOpen
                    menu: root.menu
                    stale: root.friendsState.stale
                    actionsEnabled: root.friendsState.connected
                    onToggled: root.onlineOpen = !root.onlineOpen
                }
                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("OFFLINE — %1").arg(root.friendsState.groups.offline.length)
                    friends: root.friendsState.groups.offline
                    expanded: root.offlineOpen
                    menu: root.menu
                    stale: root.friendsState.stale
                    actionsEnabled: root.friendsState.connected
                    onToggled: root.offlineOpen = !root.offlineOpen
                }

                Column {
                    visible: root.friendsState.loaded && root.friendsState.friendCount === 0
                    width: parent.width
                    spacing: Theme.space.sm

                    PanelNote {
                        width: parent.width
                        text: qsTr("Add friends who use Kite to see where they play.")
                    }
                    Button {
                        buttonSize: "sm"
                        text: qsTr("Add a friend")
                        icon.source: Icons.url("plus")
                        onClicked: root.friendsRequested("add")
                    }
                }
            }
        }

        ScrollFade { atTop: true; flickable: flick }
        ScrollFade { flickable: flick }
    }

    FriendsPageState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space.lg
        visible: !root.friendsState.showsLists
        friendsState: root.friendsState
        onNavigateRequested: (target) => root.navigateRequested(target)
    }
}
