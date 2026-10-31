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

    // FriendsController, or null.
    property var controller: null
    property var joinFlow: null
    property var menu: null

    // Derived by FriendsSidePanel from the controller, for this and the strip alike.
    property string phase: "off"
    property bool connected: false
    property bool stale: false
    property bool showsLists: false
    property bool loaded: false
    property int friendCount: 0
    // Format.friendsGrouped(): { playing, online, offline }.
    property var groups: ({ playing: [], online: [], offline: [] })

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
        ready: root.connected
        loaded: root.loaded
        friendCount: root.friendCount
        onlineCount: root.controller ? root.controller.onlineCount : 0
        incomingCount: root.controller ? root.controller.incomingCount : 0
        showsLists: root.showsLists
        onAddRequested: root.friendsRequested("add")
        onRequestsRequested: root.friendsRequested("requests")
        onCollapseRequested: root.collapseRequested()
    }

    Item {
        id: lists
        visible: root.showsLists
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
                    visible: root.stale
                    controller: root.controller
                }

                FriendsNotice {
                    width: parent.width
                    controller: root.controller
                    onSettingsRequested: root.navigateRequested("settings/friends")
                }

                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("IN GAME — %1").arg(root.groups.playing.length)
                    friends: root.groups.playing
                    expanded: root.playingOpen
                    menu: root.menu
                    joinFlow: root.joinFlow
                    stale: root.stale
                    actionsEnabled: root.connected
                    onToggled: root.playingOpen = !root.playingOpen
                    onJoinRequested: (friend) => root.joinRequested(friend)
                }
                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("ONLINE — %1").arg(root.groups.online.length)
                    friends: root.groups.online
                    expanded: root.onlineOpen
                    menu: root.menu
                    stale: root.stale
                    actionsEnabled: root.connected
                    onToggled: root.onlineOpen = !root.onlineOpen
                }
                FriendsPanelGroup {
                    width: parent.width
                    title: qsTr("OFFLINE — %1").arg(root.groups.offline.length)
                    friends: root.groups.offline
                    expanded: root.offlineOpen
                    menu: root.menu
                    stale: root.stale
                    actionsEnabled: root.connected
                    onToggled: root.offlineOpen = !root.offlineOpen
                }

                Column {
                    visible: root.loaded && root.friendCount === 0
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

        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            atTop: true
            flickable: flick
        }
        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            flickable: flick
        }
    }

    FriendsPageState {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space.lg
        visible: !root.showsLists
        controller: root.controller
        phase: root.phase
        onNavigateRequested: (target) => root.navigateRequested(target)
    }
}
