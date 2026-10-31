// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The friends list in full: you (FriendsSelfCard), then Friends, Requests and Blocked tabs. While friends
 * are not connected the list gives way to FriendsPageState; a poll that failed after the lists were read
 * keeps them under a bar, and nothing that needs the server works until it answers.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    // Your own head ("image://accountface/..."); the launcher already has it.
    property string myFace: ""
    property var joinFlow: null
    property string tab: "friends"
    // Main.qml binds this to its PageSlot's StackLayout.isCurrentItem.
    property bool shown: false
    property bool retrying: false

    signal navigateRequested(string target)
    signal joinRequested(var friend)

    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    readonly property string phase: Format.friendsPhase(root.controller)
    readonly property bool connected: root.phase === "ready"
    readonly property bool stale: Format.friendsStale(root.controller)
    readonly property bool showsLists: root.connected || root.stale
    readonly property string updatedAt: Format.friendsUpdatedAt(root.controller)
    // With no friends, requests or blocks the page is only you and the way to add someone.
    readonly property bool hasAnyone: Format.friendsHasAnyone(root.controller, true)
    readonly property int readableWidth: 880
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    // Also fills the Blocked tab's count before it is opened. Only while connected: otherwise the
    // controller answers with a toast the page already says.
    function load() {
        if (!root.controller || !root.connected)
            return
        root.controller.refresh()
        root.controller.refreshBlocked()
    }
    function focusAddField() {
        selfCard.focusField()
    }
    // @p mode: "remove", "block", or "blockRequest" (the person is not a friend: their request goes).
    function confirmAction(mode, uuid, name) {
        confirm.ask(mode, uuid, name)
    }

    onShownChanged: if (root.shown) root.load()
    onConnectedChanged: if (root.shown) root.load()
    Component.onCompleted: if (root.shown) root.load()

    Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "refresh")
                root.retrying = false
        }
        // A refresh dropped because friends was turned off meanwhile is never answered: the flag lasts while it is
        // failing.
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
        anchors.leftMargin: root.sideMargin
        anchors.topMargin: Theme.space.xl
        anchors.bottomMargin: Theme.space.lg
        width: Math.min(root.width - root.sideMargin * 2, root.readableWidth)
        spacing: Theme.space.lg

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
                // While some wait for an answer, their number; otherwise the ones you sent, so the tab never says 0
                // over rows.
                { id: "requests", label: qsTr("Requests"),
                  count: root.controller ? (root.controller.incomingCount > 0 ? root.controller.incomingCount : root.controller.outgoing.length) : 0,
                  attention: !!root.controller && root.controller.incomingCount > 0 },
                { id: "blocked", label: qsTr("Blocked"), count: root.controller ? root.controller.blocked.length : 0 }
            ]
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: !root.hasAnyone ? 0 : root.tab === "requests" ? 1 : root.tab === "blocked" ? 2 : 0

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

    FriendsPageState {
        anchors.fill: parent
        visible: !root.showsLists
        controller: root.controller
        phase: root.phase
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // One menu for the page, not one per row: the lists are rebuilt on every read and a menu in a row would close
    // under the pointer.
    FriendPersonMenu {
        id: personMenu
        onRemoveRequested: (uuid, name) => root.confirmAction("remove", uuid, name)
        onBlockRequested: (uuid, name, friend) => root.confirmAction(friend ? "block" : "blockRequest", uuid, name)
    }

    FriendConfirmDialog {
        id: confirm
        controller: root.controller
    }
}
