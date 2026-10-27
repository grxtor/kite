// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Your friends, in a narrow panel fixed on the window's right edge on every
 * page, as Lunar's launcher has it: who is in game, who is online and --
 * folded away until you open it -- who is offline, each with Join where the
 * friend lets you (FriendsPanelGroup). Removing or blocking someone is the
 * row's ⋯, with the same question the Friends page asks. What does not fit a
 * narrow column stays on the page (Main.qml's "friends"): requests, blocked
 * people and adding someone by name; the header's two buttons go there.
 *
 * Main.qml sets `open` from its top bar's toggle and the FriendsPanelOpen
 * setting; closing slides the width to nothing, and the content keeps its own
 * width while it does, so nothing reflows on the way. The panel is built once
 * and never torn down, so the folds below hold for the session.
 *
 * Friends off, signed out, connecting or failed before anything was read: the
 * Friends page's own state (FriendsPageState) stands in for the lists. A poll
 * that failed after the lists were read leaves them, under a bar saying they
 * may be out of date, and nothing that needs the server (Join, ⋯) works until
 * it answers. The first time friends connect, FriendsNotice tops the list:
 * Home's panel does not show friends while this one is out, so this is where
 * people are told what their friends can see.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    // Whether the panel is out.
    property bool open: false

    // The Friends page, on "friends", "requests" or "blocked", or "add" (the
    // add field focused).
    signal friendsRequested(string tab)
    // A path for Main.qml's navigate().
    signal navigateRequested(string target)
    // Join this friend's server (a friend map); Main.qml picks the instance.
    signal joinRequested(var friend)

    // A narrow column, as wide as Home's side panel plus what a name and
    // Join need to share a row: too narrow and a name shortens to a few
    // letters on a friend who can be joined.
    readonly property int panelWidth: 320

    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    readonly property string phase: Format.friendsPhase(root.controller)
    readonly property bool connected: root.phase === "ready"
    // The lists are shown, if not trusted: connected, or failing after they
    // were read.
    readonly property bool stale: Format.friendsStale(root.controller)
    readonly property bool showsLists: root.connected || root.stale
    // The list has been read: before that it is empty for want of an answer,
    // which says nothing about whether there are friends.
    readonly property bool loaded: !!root.controller && root.controller.loaded
    readonly property int friendCount: root.controller ? root.controller.friends.length : 0
    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])

    // Which groups show their rows. Offline is the long one and the least
    // wanted, so it starts folded.
    property bool playingOpen: true
    property bool onlineOpen: true
    property bool offlineOpen: false

    // Fresh lists whenever the panel is shown. Only while connected: asking
    // while friends are off or failing only makes the controller answer with
    // a toast saying what the panel already says. (A panel that opens before
    // friends connect has nothing to ask yet; the controller reads the lists
    // itself as it connects.)
    function load() {
        if (!root.controller || !root.connected)
            return
        root.controller.refresh()
    }
    onOpenChanged: if (root.open) root.load()
    Component.onCompleted: {
        if (root.open)
            root.load()
        // The width is set at start-up from the saved setting, and should
        // appear in place; only a toggle after that slides.
        Qt.callLater(() => root.settled = true)
    }

    // True from the first turn of the event loop.
    property bool settled: false

    width: root.open ? root.panelWidth : 0
    visible: root.width > 0
    clip: true
    Behavior on width {
        enabled: root.settled
        NumberAnimation {
            duration: SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.normal
            easing.type: Theme.motion.easing
        }
    }

    Item {
        id: content
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.panelWidth

        Rectangle {
            anchors.fill: parent
            color: Theme.palette.canvas
        }
        // Set off from the page by a hairline, as Home's side panel is, and
        // the page panel's top edge carries on across.
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Theme.palette.divider
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.palette.border
        }

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
                        menu: personMenu
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
                        menu: personMenu
                        stale: root.stale
                        actionsEnabled: root.connected
                        onToggled: root.onlineOpen = !root.onlineOpen
                    }
                    FriendsPanelGroup {
                        width: parent.width
                        title: qsTr("OFFLINE — %1").arg(root.groups.offline.length)
                        friends: root.groups.offline
                        expanded: root.offlineOpen
                        menu: personMenu
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

        // What a row's ⋯ opens. One for the panel, not one per row: the
        // lists are rebuilt whenever they are read again, and a menu in a
        // row would close under the pointer.
        FriendPersonMenu {
            id: personMenu
            onRemoveRequested: (uuid, name) => confirm.ask("remove", uuid, name)
            onBlockRequested: (uuid, name, friend) => confirm.ask(friend ? "block" : "blockRequest", uuid, name)
        }

        // Removing a friend and blocking someone, both after a question.
        FriendConfirmDialog {
            id: confirm
            controller: root.controller
        }
    }
}
