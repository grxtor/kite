// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Where a friends view stands, worked out once from the controller: the Friends page, the side panel and Home's
 * section each hold one. Not visible; the views read it instead of repeating the Format.friends*() calls.
 */
QtObject {
    id: root

    // FriendsController, or null.
    property var controller: null

    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    readonly property string phase: Format.friendsPhase(root.controller)
    readonly property bool connected: root.phase === "ready"
    readonly property bool signInFirst: Format.friendsSignInFirst(root.controller)
    readonly property bool stale: Format.friendsStale(root.controller)
    // A failed poll with the lists already read keeps them on screen.
    readonly property bool showsLists: root.connected || root.stale
    // Before the first read the lists are empty for want of an answer, not for lack of friends.
    readonly property bool loaded: !!root.controller && root.controller.loaded
    readonly property int friendCount: root.controller ? root.controller.friends.length : 0
    readonly property int onlineCount: root.controller ? root.controller.onlineCount : 0
    readonly property int incomingCount: root.controller ? root.controller.incomingCount : 0
    // Format.friendsGrouped(): { playing, online, offline }.
    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])
    // Invites to you, newest first: [{ uuid, name, face, host, port, expiresAt }]. [] until the controller has them
    // (the friends server's invites, PR #14).
    readonly property var invites: root.controller && root.controller.invites ? root.controller.invites : []

    // The invite from the friend with @p uuid, or null.
    function inviteFrom(uuid) {
        return root.invites.find((one) => one.uuid === uuid) || null
    }
    // A friend who waits on you: an invite from them, or messages you have not read.
    function wantsYou(friend) {
        return (friend.unread || 0) > 0 || !!root.inviteFrom(friend.uuid)
    }
}
