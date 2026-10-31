// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What the Friends page shows instead of its list while friends are not connected: off, connecting, signed out, or
 * failed. A failure only a new Minecraft sign-in fixes (`controller.needsSignIn`) offers "Sign in"; any other
 * offers "Try again". Once the lists have been read, a failed poll does not get here (Format.friendsStale()).
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    // "off", "noAccount", "signingIn" or "error" (Format.friendsPhase()).
    property string phase: "off"

    signal navigateRequested(string target)

    readonly property string errorText: root.controller ? root.controller.error : ""
    readonly property bool signInFirst: Format.friendsSignInFirst(root.controller)
    readonly property bool failed: root.phase === "error" && !root.signInFirst
    property bool retrying: false

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

    EmptyState {
        anchors.fill: parent
        upperThird: true
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
                  : root.failed ? (root.retrying ? qsTr("Trying…") : qsTr("Try again")) : ""
        actionIcon: root.failed && !root.retrying ? "refresh" : ""
        actionEnabled: !root.retrying
        onActionTriggered: {
            if (root.phase === "off") {
                root.controller.enabled = true
            } else if (root.signInFirst) {
                root.navigateRequested("accounts")
            } else if (root.controller) {
                root.retrying = true
                root.controller.refresh()
            }
        }

        SoftIconCircle {
            iconName: root.failed ? "alert-triangle" : root.signInFirst ? "user" : "users"
            tone: root.failed ? "warning" : "accent"
        }
    }
}
