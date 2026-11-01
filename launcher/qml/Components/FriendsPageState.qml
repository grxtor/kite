// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What the Friends page shows instead of its list while friends are not connected: off, connecting, signed out, or
 * failed. A failure only a new Minecraft sign-in fixes (`friendsState.signInFirst`) offers "Sign in"; any other
 * offers "Try again". Once the lists have been read, a failed poll does not get here (FriendsState.stale).
 */
Item {
    id: root

    // The view's FriendsState.
    property FriendsState friendsState: FriendsState {}

    signal navigateRequested(string target)

    readonly property var controller: root.friendsState.controller
    readonly property string phase: root.friendsState.phase
    readonly property string errorText: root.controller ? root.controller.error : ""
    readonly property bool signInFirst: root.friendsState.signInFirst
    readonly property bool failed: root.phase === "error" && !root.signInFirst

    FriendsRetry {
        id: retry
        controller: root.controller
    }

    EmptyState {
        anchors.fill: parent
        upperThird: true
        iconName: root.failed ? "alert-triangle" : root.signInFirst ? "user" : "users"
        iconColor: root.failed ? Theme.tone("warning").ink : Theme.palette.textTertiary
        title: root.phase === "off" ? qsTr("Friends is off")
             : root.phase === "signingIn" ? qsTr("Connecting…")
             : root.signInFirst ? qsTr("Sign in to use Friends")
             : qsTr("Can't connect")
        body: root.phase === "off" ? qsTr("See which server your friends are on, and join them when they let you. While Friends is off, Kite sends nothing.")
            : root.phase === "signingIn" ? ""
            : root.phase === "noAccount" ? Format.friendsNeedAccount
            : root.errorText.length > 0 ? root.errorText
            : qsTr("Friends could not connect.")
        actionText: root.phase === "off" ? qsTr("Turn on")
                  : root.signInFirst ? qsTr("Sign in")
                  : root.failed ? (retry.retrying ? qsTr("Trying…") : qsTr("Try again")) : ""
        actionIcon: root.failed && !retry.retrying ? "refresh" : ""
        actionEnabled: !retry.retrying
        onActionTriggered: {
            if (root.phase === "off")
                root.controller.enabled = true
            else if (root.signInFirst)
                root.navigateRequested("accounts")
            else
                retry.run()
        }
    }
}
