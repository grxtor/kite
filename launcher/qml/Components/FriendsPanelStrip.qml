// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The friends panel folded to a strip, as it starts: the buttons for what
 * you reach for from anywhere (show the full panel, the requests waiting,
 * add a friend) stacked at the top, and under them nothing but friends'
 * heads -- who is in game first, then who is online, then who is not --
 * with a hairline between the groups. A head says who it is as a tip and
 * opens a small card (FriendHeadCard) with Join and ⋯; whoever wants names
 * and groups widens the panel (the expand button, or drag its edge).
 *
 * Friends off, signed out, connecting or failed before anything was read:
 * there are no heads, so a single button stands in the strip, with the
 * state's glyph, and opens the full panel, where FriendsPageState says what
 * is wrong and what to do. A failed poll after the lists were read leaves
 * the heads, without presence dots; the bar saying so is the full panel's.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    // The panel's FriendPersonMenu, which the card's ⋯ opens.
    property var menu: null
    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    property string phase: "off"
    // The lists are shown, if not trusted: connected, or failing after they
    // were read.
    property bool showsLists: false
    property bool stale: false
    // The server answers: Join and ⋯ need it, reading the list does not.
    property bool connected: false
    // Format.friendsGrouped(): { playing, online, offline }.
    property var groups: ({ playing: [], online: [], offline: [] })
    property int incomingCount: 0
    // The colour the panel stands on, for the requests' count ring.
    property color surface: Theme.palette.canvas

    // Show the full panel.
    signal expandRequested()
    signal addRequested()
    signal requestsRequested()
    signal joinRequested(var friend)

    // The words and glyph of the one button while there are no lists: the
    // Friends page's own (FriendsPageState), so the strip and the panel it
    // opens speak alike.
    readonly property bool signInFirst: Format.friendsSignInFirst(root.controller)
    readonly property bool failed: root.phase === "error" && !root.signInFirst
    readonly property string stateTip: root.phase === "off" ? qsTr("Friends is off")
                                     : root.phase === "signingIn" ? qsTr("Connecting…")
                                     : root.signInFirst ? qsTr("Sign in to use Friends")
                                     : qsTr("Can't connect")
    readonly property string stateIcon: root.failed ? "alert-triangle" : root.signInFirst ? "user" : "users"

    // A head's card and the tip rows would cover are no use once the strip
    // is out of sight (the panel is hidden, or widened).
    onVisibleChanged: if (!root.visible) card.close()

    Column {
        id: tools
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.space.sm
        spacing: Theme.space.xs

        IconButton {
            size: Theme.control.heightSm
            visible: root.showsLists
            iconName: "chevron-left"
            tip: qsTr("Expand friends")
            onClicked: root.expandRequested()
        }
        IconButton {
            id: requests
            size: Theme.control.heightSm
            visible: root.showsLists
            iconName: "bell"
            tip: qsTr("Friend requests")
            Accessible.description: root.incomingCount > 0 ? String(root.incomingCount) : ""
            onClicked: root.requestsRequested()

            // On the corner, a little out of it, so it covers less of the glyph.
            CountPill {
                x: requests.width - width + Theme.space.xs
                y: -Theme.space.xs
                count: root.incomingCount
                ringColor: root.surface
            }
        }
        IconButton {
            size: Theme.control.heightSm
            visible: root.showsLists
            iconName: "plus"
            tip: qsTr("Add a friend")
            onClicked: root.addRequested()
        }
        IconButton {
            size: Theme.control.heightSm
            visible: !root.showsLists
            iconName: root.stateIcon
            tip: root.stateTip
            onClicked: root.expandRequested()
        }
    }

    Rectangle {
        id: rule
        visible: root.showsLists
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: tools.bottom
        anchors.topMargin: Theme.space.sm
        width: Theme.space.xl
        height: 1
        color: Theme.palette.divider
    }

    Item {
        id: lists
        visible: root.showsLists
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rule.bottom
        anchors.topMargin: Theme.space.sm
        anchors.bottom: parent.bottom

        Flickable {
            id: flick
            anchors.fill: parent
            contentWidth: width
            contentHeight: heads.height + Theme.space.xl
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            // The card is placed beside the head it was opened on, which
            // scrolling would leave behind.
            onMovementStarted: card.close()

            ScrollBar.vertical: ScrollBar {}

            Column {
                id: heads
                x: Theme.space.sm
                width: flick.width - Theme.space.sm * 2
                spacing: Theme.space.xs

                Repeater {
                    model: root.groups.playing
                    delegate: headDelegate
                }
                GroupRule {
                    visible: root.groups.playing.length > 0 && root.groups.online.length > 0
                }
                Repeater {
                    model: root.groups.online
                    delegate: headDelegate
                }
                GroupRule {
                    visible: root.groups.playing.length + root.groups.online.length > 0
                             && root.groups.offline.length > 0
                }
                Repeater {
                    model: root.groups.offline
                    delegate: headDelegate
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

    // The thin line between the in game, online and offline heads: the
    // strip has no captions to say where a group ends.
    component GroupRule: Item {
        // The heads' column.
        width: parent ? parent.width : 0
        height: Theme.space.sm
        Rectangle {
            anchors.centerIn: parent
            width: Theme.space.xl
            height: 1
            color: Theme.palette.divider
        }
    }

    Component {
        id: headDelegate

        FriendsStripHead {
            id: head
            required property var modelData

            width: heads.width
            friend: head.modelData
            stale: root.stale
            selected: card.opened && card.friendUuid === head.modelData.uuid
            onClicked: card.openFor(head, head.modelData.uuid)
        }
    }

    FriendHeadCard {
        id: card
        controller: root.controller
        menu: root.menu
        joinFlow: root.joinFlow
        stale: root.stale
        actionsEnabled: root.connected
        onJoinRequested: (friend) => root.joinRequested(friend)
    }
}
