// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What waits on you, above a friends list: invites to a server (FriendInviteCard) and, with `withRequests`, friend
 * requests to accept or decline (FriendRequestRow). Shows nothing while there is none or while not connected.
 * Invites come with the friends server's invites (PR #14): until FriendsController has them the list is empty.
 */
Column {
    id: root

    property FriendsState friendsState: FriendsState {}
    // Main.qml's FriendPlaces.
    property FriendPlaces places: FriendPlaces {}
    property var joinFlow: null
    property bool withRequests: false

    signal joinRequested(var friend)

    readonly property var controller: root.friendsState.controller
    readonly property var invites: root.friendsState.connected ? root.friendsState.invites : []
    readonly property var requests: root.withRequests && root.friendsState.connected && root.controller
                                    ? root.controller.incoming : []

    function inviterOf(invite) {
        return (root.controller ? root.controller.friends : []).find((one) => one.uuid === invite.uuid) || ({})
    }
    // What Join on @p invite joins, as a friend map for FriendJoinFlow: the invite's server, with the inviter's game
    // version to pick the instance. Not joinable while that version is unknown (they are not in a game).
    function joinTarget(invite) {
        var inviter = root.inviterOf(invite)
        return {
            uuid: invite.uuid,
            name: invite.name,
            state: "playing",
            worldType: "server",
            host: invite.host,
            port: invite.port,
            version: inviter.version || "",
            instance: inviter.instance || "",
            joinable: !!inviter.version
        }
    }

    visible: root.invites.length + root.requests.length > 0
    spacing: Theme.space.sm

    FriendsPending {
        id: pending
        controller: root.controller
        actions: ["accept", "decline"]
    }

    Repeater {
        model: root.invites
        delegate: FriendInviteCard {
            id: card
            required property var modelData

            width: root.width
            invite: modelData
            target: root.joinTarget(modelData)
            place: root.places.of(card.target)
            presence: root.inviterOf(modelData).state || ""
            joinFlow: root.joinFlow
            onJoinRequested: root.joinRequested(card.target)
            // FriendsController gains dismissInvite() with the invites themselves.
            onDismissRequested: {
                if (root.controller && typeof root.controller.dismissInvite === "function")
                    root.controller.dismissInvite(card.modelData.uuid)
            }
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
