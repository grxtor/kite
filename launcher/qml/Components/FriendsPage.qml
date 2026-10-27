// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The friends list in full, a page of its own that the friends panel on the
 * window's right edge (FriendsSidePanel) opens for what does not fit a narrow
 * column: you (FriendsSelfCard, with where to add someone by their Minecraft
 * name), then three tabs -- Friends (where each one is, with Join where the
 * friend allows it; ⋯ to remove or block), Requests (accept, decline,
 * cancel) and Blocked (unblock). Removing and blocking ask first. What the
 * server answered comes back as a toast from Main.qml, off
 * FriendsController.actionFinished. While friends are not connected the
 * list gives way to FriendsPageState, except for a poll that failed after
 * the lists were read: those stay, with a bar saying they may be out of
 * date, and nothing that needs the server (Add, Join, ⋯) works until it
 * answers.
 *
 * The column hangs from the page title, as every page's content does, and
 * stops growing at a readable width.
 *
 * The first time friends connect, FriendsNotice tops the page.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Your own head ("image://accountface/...", QmlShell.accountFace): the
    // launcher already has it, so it is not fetched again.
    property string myFace: ""
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    // "friends", "requests" or "blocked".
    property string tab: "friends"
    // Whether this is the page on screen. Main.qml binds it to its PageSlot's
    // StackLayout.isCurrentItem (see DiscoverPage.shown).
    property bool shown: false
    // "Try again" in the bar was pressed and has not been answered.
    property bool retrying: false

    // A path for Main.qml's navigate().
    signal navigateRequested(string target)
    // Join this friend's server (a friend map); Main.qml picks the instance.
    signal joinRequested(var friend)

    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    readonly property string phase: Format.friendsPhase(root.controller)
    readonly property bool connected: root.phase === "ready"
    // The lists are shown, if not trusted: connected, or failing after they
    // were read.
    readonly property bool stale: Format.friendsStale(root.controller)
    readonly property bool showsLists: root.connected || root.stale
    // When the lists shown were last read, as a time of day ("" if never).
    readonly property string updatedAt: Format.friendsUpdatedAt(root.controller)
    // Something to tab between: with no friends, requests or blocks the page
    // is only you and the way to add someone.
    readonly property bool hasAnyone: Format.friendsHasAnyone(root.controller, true)
    // Past this the list stops reading as one block; the column stops
    // growing here on wide windows, as SettingsScroll's does.
    readonly property int readableWidth: 880
    // Where the content starts: under the top bar's title (TopBar.sideMargin).
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    function openTab(id) {
        root.tab = id
    }
    // Fresh lists whenever the page is shown, and the Blocked tab's count
    // before it is ever opened. Only while connected: asking while friends
    // are off or failing only makes the controller answer with a toast
    // saying what the page already says.
    function load() {
        if (!root.controller || !root.connected)
            return
        root.controller.refresh()
        root.controller.refreshBlocked()
    }
    // The add field, focused: Home's "Add a friend".
    function focusAddField() {
        selfCard.focusField()
    }
    // @p mode: "remove" a friend, "block" a friend, or "blockRequest" (the
    // person is not a friend: their request is what goes).
    function confirmAction(mode, uuid, name) {
        confirm.ask(mode, uuid, name)
    }

    onShownChanged: if (root.shown) root.load()
    // Connecting while the page is open (after Turn on, or Try again).
    onConnectedChanged: if (root.shown) root.load()
    Component.onCompleted: if (root.shown) root.load()

    Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "refresh")
                root.retrying = false
        }
        // A refresh dropped because friends was turned off meanwhile is
        // never answered: the flag is for as long as it is failing.
        function onChanged() {
            if (root.controller.state !== "error")
                root.retrying = false
        }
    }

    ColumnLayout {
        id: content
        visible: root.showsLists
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        // The page area gives no padding of its own.
        anchors.leftMargin: root.sideMargin
        anchors.topMargin: Theme.space.xl
        anchors.bottomMargin: Theme.space.lg
        width: Math.min(root.width - root.sideMargin * 2, root.readableWidth)
        spacing: Theme.space.lg

        // The last lists, which may be old. The retry is the button, so the
        // words do not also say "try again"; what the server said is the tip.
        NoticeBar {
            id: staleBar
            Layout.fillWidth: true
            visible: root.stale
            text: root.updatedAt.length > 0
                  ? qsTr("Can't update friends right now. Showing them as of %1.").arg(root.updatedAt)
                  : qsTr("Can't update friends right now.")
            actionText: root.retrying ? qsTr("Trying…") : qsTr("Try again")
            onActionTriggered: {
                if (root.retrying)
                    return
                root.retrying = true
                root.controller.refresh()
            }

            HoverHandler { id: staleHover }
            ToolTip.visible: staleHover.hovered && !!root.controller && root.controller.error.length > 0
            ToolTip.delay: 500
            ToolTip.text: root.controller ? root.controller.error : ""
        }

        FriendsNotice {
            Layout.fillWidth: true
            controller: root.controller
            onSettingsRequested: root.navigateRequested("settings/friends")
        }

        FriendsSelfCard {
            id: selfCard
            Layout.fillWidth: true
            controller: root.controller
            face: root.myFace
            online: root.connected
        }

        TabStrip {
            Layout.fillWidth: true
            visible: root.hasAnyone
            current: root.tab
            onActivated: (id) => root.tab = id
            tabs: [
                { id: "friends", label: qsTr("Friends"), count: root.controller ? root.controller.friends.length : 0 },
                // The number the rail shows while some wait for an answer (on
                // an accent pill, as there); otherwise the ones you sent, so
                // the tab never says 0 over rows.
                { id: "requests", label: qsTr("Requests"),
                  count: root.controller ? (root.controller.incomingCount > 0 ? root.controller.incomingCount : root.controller.outgoing.length) : 0,
                  attention: !!root.controller && root.controller.incomingCount > 0 },
                { id: "blocked", label: qsTr("Blocked"), count: root.controller ? root.controller.blocked.length : 0 }
            ]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Nothing to tab between: the Friends tab's invitation.
            currentIndex: !root.hasAnyone ? 0 : root.tab === "requests" ? 1 : root.tab === "blocked" ? 2 : 0

            // The last lists are for reading: what needs the server waits
            // (the rows' buttons), and the lists still scroll.
            FriendsFriendsTab {
                controller: root.controller
                menu: personMenu
                joinFlow: root.joinFlow
                stale: root.stale
                actionsEnabled: root.connected
                onJoinRequested: (friend) => root.joinRequested(friend)
            }
            FriendsRequestsTab {
                controller: root.controller
                menu: personMenu
                actionsEnabled: root.connected
            }
            FriendsBlockedTab {
                controller: root.controller
                actionsEnabled: root.connected
            }
        }
    }

    // Friends off, signed out, connecting or failed before anything was
    // read: nothing to list.
    FriendsPageState {
        anchors.fill: parent
        visible: !root.showsLists
        controller: root.controller
        phase: root.phase
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // What a row's ⋯ opens. One for the page, not one per row: the lists are
    // rebuilt whenever they are read again, and a menu in a row would close
    // under the pointer.
    FriendPersonMenu {
        id: personMenu
        onRemoveRequested: (uuid, name) => root.confirmAction("remove", uuid, name)
        onBlockRequested: (uuid, name, friend) => root.confirmAction(friend ? "block" : "blockRequest", uuid, name)
    }

    // Removing a friend and blocking someone, both after a question.
    FriendConfirmDialog {
        id: confirm
        controller: root.controller
    }
}
