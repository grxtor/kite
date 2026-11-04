// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The friends panel folded to a strip: buttons at the top (show the full panel, requests waiting, add a friend),
 * then friends' heads with a hairline between groups: first whoever waits on you (an invite, unread messages, as
 * Discord lifts a chat with news), then in game, in Kite, offline. A head opens the friend's card
 * (FriendHeadCard). With no lists one button stands in and opens the full panel.
 */
Item {
    id: root

    // FriendsSidePanel's FriendsState.
    property FriendsState friendsState: FriendsState {}
    property var joinFlow: null
    property var menu: null
    // Main.qml's FriendPlaces, for the server names and favicons.
    property FriendPlaces places: FriendPlaces {}
    property color surface: Theme.palette.canvas

    signal expandRequested()
    signal addRequested()
    signal requestsRequested()
    signal joinRequested(var friend)

    readonly property bool failed: root.friendsState.phase === "error" && !root.friendsState.signInFirst
    readonly property string stateTip: root.friendsState.phase === "off" ? qsTr("Friends is off")
                                     : root.friendsState.phase === "signingIn" ? qsTr("Connecting…")
                                     : root.friendsState.signInFirst ? qsTr("Sign in to use Friends")
                                     : qsTr("Can't connect")
    readonly property string stateIcon: root.failed ? "alert-triangle" : root.friendsState.signInFirst ? "user" : "users"

    readonly property var ordered: {
        var groups = root.friendsState.groups
        var all = groups.playing.concat(groups.online, groups.offline)
        return all.filter((one) => root.friendsState.wantsYou(one))
                  .concat(all.filter((one) => !root.friendsState.wantsYou(one)))
    }
    // 0 waits on you, 1 in game, 2 in Kite, 3 offline.
    function groupOf(friend) {
        return root.friendsState.wantsYou(friend) ? 0
             : friend.state === "playing" ? 1 : friend.state === "online" ? 2 : 3
    }

    function toggleCard(head, uuid) {
        if (card.opened && card.friendUuid === uuid)
            card.close()
        else
            card.openFor(head, uuid)
    }

    // A head's card is no use once the strip is out of sight and would be left floating.
    onVisibleChanged: if (!root.visible) card.close()

    // A press on these buttons is inside the strip, which the card does not close for, so each closes it itself.
    Column {
        id: tools
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.space.sm
        spacing: Theme.space.xs

        IconButton {
            size: Theme.control.heightSm
            visible: root.friendsState.showsLists
            iconName: "chevron-left"
            tip: qsTr("Expand friends")
            onClicked: {
                card.close()
                root.expandRequested()
            }
        }
        IconButton {
            id: requests
            size: Theme.control.heightSm
            visible: root.friendsState.showsLists
            iconName: "bell"
            tip: qsTr("Friend requests")
            Accessible.description: root.friendsState.incomingCount > 0 ? String(root.friendsState.incomingCount) : ""
            onClicked: {
                card.close()
                root.requestsRequested()
            }

            CountPill {
                x: requests.width - width + Theme.space.xs
                y: -Theme.space.xs
                count: root.friendsState.incomingCount
                ringColor: root.surface
            }
        }
        IconButton {
            size: Theme.control.heightSm
            visible: root.friendsState.showsLists
            iconName: "plus"
            tip: qsTr("Add a friend")
            onClicked: {
                card.close()
                root.addRequested()
            }
        }
        IconButton {
            size: Theme.control.heightSm
            visible: !root.friendsState.showsLists
            iconName: root.stateIcon
            tip: root.stateTip
            onClicked: root.expandRequested()
        }
    }

    Rectangle {
        id: rule
        visible: root.friendsState.showsLists
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: tools.bottom
        anchors.topMargin: Theme.space.sm
        width: Theme.space.xl
        height: 1
        color: Theme.palette.divider
    }

    Item {
        id: lists
        visible: root.friendsState.showsLists
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
            // The card is placed beside the head it was opened on, which scrolling would leave behind.
            onMovementStarted: card.close()

            ScrollBar.vertical: ScrollBar {}

            Column {
                id: heads
                x: Theme.space.sm
                width: flick.width - Theme.space.sm * 2
                spacing: Theme.space.xs

                Repeater {
                    model: root.ordered
                    delegate: Column {
                        id: slot
                        required property var modelData
                        required property int index

                        width: heads.width
                        spacing: Theme.space.xs

                        GroupRule {
                            visible: slot.index > 0
                                     && root.groupOf(slot.modelData) !== root.groupOf(root.ordered[slot.index - 1])
                        }
                        FriendsStripHead {
                            id: head
                            width: slot.width
                            friend: slot.modelData
                            place: root.places.of(slot.modelData)
                            invited: !!root.friendsState.inviteFrom(slot.modelData.uuid)
                            stale: root.friendsState.stale
                            selected: card.opened && card.friendUuid === slot.modelData.uuid
                            onClicked: root.toggleCard(head, slot.modelData.uuid)
                        }
                    }
                }
            }
        }

        ScrollFade { atTop: true; flickable: flick }
        ScrollFade { flickable: flick }
    }

    component GroupRule: Item {
        width: parent ? parent.width : 0
        height: Theme.space.sm
        Rectangle {
            anchors.centerIn: parent
            width: Theme.space.xl
            height: 1
            color: Theme.palette.divider
        }
    }

    FriendHeadCard {
        id: card
        controller: root.friendsState.controller
        places: root.places
        invites: root.friendsState.invites
        menu: root.menu
        joinFlow: root.joinFlow
        stale: root.friendsState.stale
        actionsEnabled: root.friendsState.connected
        onJoinRequested: (friend) => root.joinRequested(friend)
    }
}
