// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The friends panel opened out: a header (FriendsPanelHeader), then who is
 * in game, who is online and -- folded away until you open it -- who is
 * offline, each with Join where the friend lets you (FriendsPanelGroup).
 * Removing or blocking someone is the row's ⋯, whose menu is the panel's
 * (`menu`). What does not fit a column stays on the Friends page:
 * requests, blocked people and adding someone by name; the header's two
 * buttons go there.
 *
 * Friends off, signed out, connecting or failed before anything was read:
 * the Friends page's own state (FriendsPageState) stands in for the lists. A
 * poll that failed after the lists were read leaves them, under a bar saying
 * they may be out of date, and nothing that needs the server (Join, ⋯) works
 * until it answers. The first time friends connect, FriendsNotice tops the
 * list: Home's panel does not show friends while this one is opened out, so
 * this is where people are told what their friends can see.
 *
 * FriendsSidePanel builds this once and never tears it down, so the folds
 * below hold for the session; it lays this out at the width the panel has
 * (or is growing to), and leaves it as it is while the panel slides.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    // The panel's FriendPersonMenu, which every row's ⋯ opens.
    property var menu: null

    // What FriendsSidePanel works out from the controller, for this and the
    // strip alike: "off", "noAccount", "signingIn", "error" or "ready"
    // (Format.friendsPhase()); the server answers; the lists are shown, if
    // not trusted; the list has been read; and how many friends there are.
    property string phase: "off"
    property bool connected: false
    property bool stale: false
    property bool showsLists: false
    property bool loaded: false
    property int friendCount: 0
    // Format.friendsGrouped(): { playing, online, offline }.
    property var groups: ({ playing: [], online: [], offline: [] })

    // Which groups show their rows. Offline is the long one and the least
    // wanted, so it starts folded.
    property bool playingOpen: true
    property bool onlineOpen: true
    property bool offlineOpen: false

    // The Friends page, on "friends", "requests" or "blocked", or "add" (the
    // add field focused).
    signal friendsRequested(string tab)
    // A path for Main.qml's navigate().
    signal navigateRequested(string target)
    // Join this friend's server (a friend map); Main.qml picks the instance.
    signal joinRequested(var friend)
    // Fold the panel back to the strip.
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

                // No friends at all (once the list has arrived).
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

    // Friends off, signed out, connecting or failed before anything was
    // read: nothing to list. The page's own words for it.
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
