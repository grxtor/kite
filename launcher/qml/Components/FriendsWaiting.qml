// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What waits on you, above a friends list: invites to a server (FriendInviteCard) and, with `withRequests`, friend
 * requests to accept or decline (FriendRequestRow). Shows nothing while there is none or while not connected. An
 * invite's Join goes through `joinFlow` (FriendJoinFlow.joinInvite()), × dismisses it.
 */
Column {
    id: root

    property FriendsState friendsState: FriendsState {}
    // Main.qml's FriendPlaces.
    property FriendPlaces places: FriendPlaces {}
    property var joinFlow: null
    property bool withRequests: false

    readonly property var controller: root.friendsState.controller
    readonly property var invites: root.friendsState.connected ? root.friendsState.invites : []
    readonly property var requests: root.withRequests && root.friendsState.connected && root.controller
                                    ? root.controller.incoming : []

    function inviterOf(invite) {
        return (root.controller ? root.controller.friends : []).find((one) => one.uuid === invite.uuid) || ({})
    }
    // Where @p invite goes, as FriendPlaces reads a friend: a server with the invite's host and port.
    function placeOf(invite) {
        return root.places.of({ state: "playing", worldType: "server", host: invite.host, port: invite.port })
    }

    visible: root.invites.length + root.requests.length > 0
    spacing: Theme.space.sm

    FriendsPending {
        id: pending
        controller: root.controller
        actions: ["accept", "decline", "dismissInvite"]
    }

    Repeater {
        model: root.invites
        delegate: FriendInviteCard {
            id: card
            required property var modelData

            width: root.width
            invite: modelData
            place: root.placeOf(modelData)
            presence: root.inviterOf(modelData).state || ""
            joinFlow: root.joinFlow
            busy: pending.has(modelData.uuid)
            onJoinRequested: if (root.joinFlow) root.joinFlow.joinInvite(card.modelData)
            onDismissRequested: pending.send(card.modelData.uuid, () => root.controller.dismissInvite(card.modelData.uuid))
        }
    }

    Repeater {
        model: root.requests
        // The rows' fill reaches past the column, as the groups' rows do, so heads line up with the captions.
        delegate: Item {
            id: slot
            required property var modelData

            width: root.width
            height: request.height

            FriendRequestRow {
                id: request
                readonly property string uuid: slot.modelData.uuid

                x: -Theme.space.sm
                width: slot.width + Theme.space.sm * 2
                person: slot.modelData
                busy: pending.has(request.uuid)
                onAcceptRequested: pending.send(request.uuid, () => root.controller.accept(request.uuid))
                onDeclineRequested: pending.send(request.uuid, () => root.controller.decline(request.uuid))
            }
        }
    }
}
