// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Friends" on Home's side panel, after "Set up Kite": who is around right
 * now. Ready, it is the heading with how many are online, the one-time
 * FriendsNotice, a line for waiting requests, and the first few friends who
 * are playing or online (FriendStatusRows, a Join where the friend allows
 * it); with no one online, the offline heads; with no friends, an
 * invitation to add one. Signed out, connecting or failed, it is a line
 * saying so and the thing to press; a poll that fails after the friends
 * were read keeps them, under a line saying they cannot be updated.
 * Off, it is not there at all: turning Friends off was the user's choice,
 * and Home does not ask them to undo it (the Friends page and Settings do).
 * Neither is it there for an account Minecraft will not let play
 * multiplayer: nothing here can help, and the Friends page says why.
 * "All" and every row open the Friends page (Main.qml opens it, through
 * manageRequested).
 */
Column {
    id: root

    // QmlShell.friends (FriendsController), or null where there is none.
    property var controller: null

    signal manageRequested(string tab)
    signal signInRequested()
    // A path for Main.qml's navigate().
    signal navigateRequested(string target)
    // Join this friend's server (a friend map).
    signal joinRequested(var friend)

    // Main.qml's FriendJoinFlow, which says what Join starts.
    property var joinFlow: null

    // The "Set up Kite" card above already asks to sign in, so this only
    // says why it matters, without a second button for the same step.
    property bool setupShowsSignIn: false

    // "off", "noAccount", "signingIn", "error" or "ready".
    readonly property string phase: Format.friendsPhase(root.controller)
    // Signed out, or failed in a way only signing in again fixes.
    readonly property bool signInFirst: Format.friendsSignInFirst(root.controller)
    readonly property bool ready: root.phase === "ready"
    // The friends are shown, if not trusted: ready, or failing after they
    // were read.
    readonly property bool stale: Format.friendsStale(root.controller)
    readonly property bool showsLists: root.ready || root.stale
    // Whether the section has anything to show; the panel hides without it.
    // Friends off is a choice, not something to offer to undo here; and no
    // Microsoft account (offline players chose that) is already said, with
    // the way to fix it, by the "Set up Kite" card.
    readonly property bool shown: root.phase !== "off"
                                  && !(root.phase === "noAccount" && root.setupShowsSignIn)
                                  && !(root.controller && root.controller.refused)
    readonly property int maxRows: 5
    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])
    // Playing first, then online.
    readonly property var around: root.groups.playing.concat(root.groups.online)
    readonly property int incomingCount: root.controller ? root.controller.incomingCount : 0
    readonly property int onlineCount: root.controller ? root.controller.onlineCount : 0
    // The list has been read: before that it is empty for want of an
    // answer, which says nothing about whether there are friends.
    readonly property bool loaded: !!root.controller && root.controller.loaded
    // "Try again" was pressed and has not been answered.
    property bool retrying: false

    spacing: Theme.space.sm

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

    function retry() {
        if (root.retrying)
            return
        root.retrying = true
        root.controller.refresh()
    }

    RowLayout {
        width: parent.width
        spacing: Theme.space.sm

        HomePanelHeading {
            Layout.fillWidth: false
            Layout.maximumWidth: parent.width
            text: qsTr("Friends")
        }
        Text {
            visible: root.ready && root.onlineCount > 0
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("%1 online").arg(root.onlineCount)
            color: Theme.palette.success
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
        Item { Layout.fillWidth: true }
        // "All" of nothing says nothing; with no one, the way to add is below.
        LinkButton {
            visible: root.showsLists && Format.friendsHasAnyone(root.controller, false)
            text: qsTr("All")
            onClicked: root.manageRequested("friends")
        }
    }

    /*
     * Not connected and nothing read: one line of what is going on and,
     * where there is one, the thing to press.
     */
    Column {
        visible: !root.showsLists
        width: parent.width
        spacing: Theme.space.sm

        PanelNote {
            width: parent.width
            text: root.phase === "noAccount" ? Format.friendsNeedAccount
                : root.phase === "error" ? (root.controller.error.length > 0 ? root.controller.error : qsTr("Friends could not connect."))
                : qsTr("Connecting…")
        }
        Button {
            visible: (root.phase === "noAccount" || root.phase === "error")
                     && !(root.signInFirst && root.setupShowsSignIn)
            enabled: !root.retrying
            buttonSize: "sm"
            text: root.signInFirst ? qsTr("Sign in") : root.retrying ? qsTr("Trying…") : qsTr("Try again")
            icon.source: root.signInFirst || root.retrying ? "" : Icons.url("refresh")
            onClicked: {
                if (root.signInFirst)
                    root.signInRequested()
                else
                    root.retry()
            }
        }
    }

    // A poll failed with the friends already read: they stay, this says
    // they may be out of date. The retry is the button, so the words do not
    // also say "try again"; what the server said is the tip.
    Column {
        visible: root.stale
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
        // The same small button the panel's other failure has.
        Button {
            enabled: !root.retrying
            buttonSize: "sm"
            text: root.retrying ? qsTr("Trying…") : qsTr("Try again")
            icon.source: root.retrying ? "" : Icons.url("refresh")
            onClicked: root.retry()
        }
    }

    FriendsNotice {
        width: parent.width
        controller: root.controller
        onSettingsRequested: root.navigateRequested("settings/friends")
    }

    // Waiting requests: they want an answer, so they come before the rows.
    HomeFriendsRequestsLink {
        visible: root.ready && root.incomingCount > 0
        x: -Theme.space.sm
        width: parent.width + Theme.space.sm * 2
        count: root.incomingCount
        onClicked: root.manageRequested("requests")
    }

    Repeater {
        model: root.showsLists ? root.around.slice(0, root.maxRows) : []
        delegate: Item {
            id: slot
            required property var modelData
            width: root.width
            height: row.height

            // The head stands on the column's edge, with the heading; the
            // row's hover bleeds out by its own padding.
            FriendStatusRow {
                id: row
                x: -Theme.space.sm
                width: root.width + Theme.space.sm * 2
                friend: slot.modelData
                stale: root.stale
                compact: true
                interactive: true
                onClicked: root.manageRequested("friends")

                // Starting the game: the play glyph Continue playing uses.
                IconButton {
                    id: joinButton
                    visible: Format.friendJoinable(slot.modelData)
                    enabled: root.ready
                    size: Theme.control.heightSm
                    variant: "secondary"
                    iconName: "play"
                    // What it will start, worked out as the pointer arrives.
                    tip: joinButton.hovered && root.joinFlow ? root.joinFlow.tip(slot.modelData)
                                                             : qsTr("Join %1").arg(slot.modelData.host)
                    onClicked: root.joinRequested(slot.modelData)
                    // A button only takes Space; Return would pass on to the
                    // row, which opens the Friends page.
                    Keys.onReturnPressed: joinButton.clicked()
                    Keys.onEnterPressed: joinButton.clicked()
                }
            }
        }
    }

    AbstractButton {
        id: more
        visible: root.showsLists && root.around.length > root.maxRows
        hoverEnabled: true
        implicitHeight: Theme.control.heightSm
        text: qsTr("+%1 more").arg(root.around.length - root.maxRows)
        onClicked: root.manageRequested("friends")
        contentItem: Text {
            text: more.text
            color: more.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Theme.type.label.weight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.radius.sm
            color: "transparent"
            border.width: more.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
        }
    }

    HomeFriendsOfflineStrip {
        visible: root.showsLists && root.groups.offline.length > 0 && root.around.length === 0
        width: parent.width
        friends: root.groups.offline
        onClicked: root.manageRequested("friends")
    }

    // No friends at all (once the list has arrived).
    Column {
        visible: root.showsLists && root.loaded
                 && root.groups.playing.length + root.groups.online.length + root.groups.offline.length === 0
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
            // To the page with its add field focused.
            onClicked: root.manageRequested("add")
        }
    }
}
