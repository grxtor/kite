// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Friends" on Home's side panel: who is around. Ready: online count, FriendsNotice, waiting requests, the first
 * few friends playing or online (offline heads, or an invitation to add). Not ready: a line saying so and the
 * thing to press. Absent when Friends is off or the account cannot play multiplayer.
 */
Column {
    id: root

    // FriendsController, or null.
    property var controller: null
    // Where friends play (Main.qml's FriendPlaces): server names and favicons.
    property FriendPlaces places: FriendPlaces {}

    signal manageRequested(string tab)
    signal signInRequested()
    signal navigateRequested(string target)
    signal joinRequested(var friend)

    property var joinFlow: null

    // The "Set up Kite" card above already asks to sign in; this only says why it matters.
    property bool setupShowsSignIn: false

    readonly property FriendsState friendsState: FriendsState { controller: root.controller }
    // Whether the section has anything to show; the panel hides without it. Friends off is a choice, and a
    // missing Microsoft account is already said by the "Set up Kite" card.
    readonly property bool shown: root.friendsState.phase !== "off"
                                  && !(root.friendsState.phase === "noAccount" && root.setupShowsSignIn)
                                  && !(root.controller && root.controller.refused)
    readonly property int maxRows: 5
    readonly property var around: root.friendsState.groups.playing.concat(root.friendsState.groups.online)

    spacing: Theme.space.sm

    FriendsRetry {
        id: retry
        controller: root.controller
    }

    RowLayout {
        width: parent.width
        spacing: Theme.space.sm

        HomePanelHeading {
            Layout.fillWidth: false
            Layout.maximumWidth: parent.width
            text: qsTr("Friends")
        }
        Copy {
            visible: root.friendsState.connected && root.friendsState.onlineCount > 0
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("%1 online").arg(root.friendsState.onlineCount)
            role: "label"
            tone: "success"
        }
        Item { Layout.fillWidth: true }
        LinkButton {
            visible: root.friendsState.showsLists && Format.friendsHasAnyone(root.controller, false)
            text: qsTr("All")
            onClicked: root.manageRequested("friends")
        }
    }

    Column {
        visible: !root.friendsState.showsLists
        width: parent.width
        spacing: Theme.space.sm

        PanelNote {
            width: parent.width
            text: root.friendsState.phase === "noAccount" ? Format.friendsNeedAccount
                : root.friendsState.phase === "error" ? (root.controller.error.length > 0 ? root.controller.error : qsTr("Friends could not connect."))
                : qsTr("Connecting…")
        }
        Button {
            visible: (root.friendsState.phase === "noAccount" || root.friendsState.phase === "error")
                     && !(root.friendsState.signInFirst && root.setupShowsSignIn)
            enabled: !retry.retrying
            buttonSize: "sm"
            text: root.friendsState.signInFirst ? qsTr("Sign in") : retry.retrying ? qsTr("Trying…") : qsTr("Try again")
            icon.source: root.friendsState.signInFirst || retry.retrying ? "" : Icons.url("refresh")
            onClicked: {
                if (root.friendsState.signInFirst)
                    root.signInRequested()
                else
                    retry.run()
            }
        }
    }

    // A poll failed with the friends already read: they stay, this says they may be out of date.
    Column {
        visible: root.friendsState.stale
        width: parent.width
        spacing: Theme.space.sm

        PanelNote {
            width: parent.width
            color: Theme.palette.warning
            text: qsTr("Can't update friends right now.")

            HoverHandler { id: staleHover }
            ToolTip.visible: staleHover.hovered && !!root.controller && root.controller.error.length > 0
            ToolTip.delay: 500
            ToolTip.text: root.controller ? root.controller.error : ""
        }
        Button {
            enabled: !retry.retrying
            buttonSize: "sm"
            text: retry.retrying ? qsTr("Trying…") : qsTr("Try again")
            icon.source: retry.retrying ? "" : Icons.url("refresh")
            onClicked: retry.run()
        }
    }

    FriendsNotice {
        width: parent.width
        controller: root.controller
        onSettingsRequested: root.navigateRequested("settings/friends")
    }

    FriendsWaiting {
        width: parent.width
        friendsState: root.friendsState
        places: root.places
        joinFlow: root.joinFlow
    }

    HomeFriendsRequestsLink {
        visible: root.friendsState.connected && root.friendsState.incomingCount > 0
        x: -Theme.space.sm
        width: parent.width + Theme.space.sm * 2
        count: root.friendsState.incomingCount
        onClicked: root.manageRequested("requests")
    }

    Repeater {
        model: root.friendsState.showsLists ? root.around.slice(0, root.maxRows) : []
        delegate: Item {
            id: slot
            required property var modelData
            width: root.width
            height: row.height

            FriendStatusRow {
                id: row
                x: -Theme.space.sm
                width: root.width + Theme.space.sm * 2
                friend: slot.modelData
                stale: root.friendsState.stale
                place: root.places.of(slot.modelData)
                compact: true
                interactive: true
                onClicked: root.manageRequested("friends")

                FriendJoinButton {
                    id: joinButton
                    enabled: root.friendsState.connected
                    iconOnly: true
                    friend: slot.modelData
                    joinFlow: root.joinFlow
                    onClicked: root.joinRequested(slot.modelData)
                    // A button only takes Space; Return would pass on to the row, which opens the Friends page.
                    Keys.onReturnPressed: joinButton.clicked()
                    Keys.onEnterPressed: joinButton.clicked()
                }
            }
        }
    }

    AbstractButton {
        id: more
        visible: root.friendsState.showsLists && root.around.length > root.maxRows
        hoverEnabled: true
        implicitHeight: Theme.control.heightSm
        text: qsTr("+%1 more").arg(root.around.length - root.maxRows)
        onClicked: root.manageRequested("friends")
        contentItem: Copy {
            text: more.text
            role: "label"
            tone: more.hovered ? "primary" : "secondary"
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
        }
        background: Item {
            FocusFrame { around: 0; cornerRadius: Theme.radius.sm; visible: more.visualFocus }
        }
    }

    HomeFriendsOfflineStrip {
        visible: root.friendsState.showsLists && root.friendsState.groups.offline.length > 0 && root.around.length === 0
        width: parent.width
        friends: root.friendsState.groups.offline
        onClicked: root.manageRequested("friends")
    }

    Column {
        visible: root.friendsState.showsLists && root.friendsState.loaded && root.friendsState.friendCount === 0
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
            onClicked: root.manageRequested("add")
        }
    }
}
