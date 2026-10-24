// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What the Friends page shows instead of its list while friends are not
 * connected: off (with the Turn on that asks the question, FriendsOptInDialog),
 * still connecting, signed out, or failed. The wording follows HomeFriends,
 * which says the same things in its side panel.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // "off", "noAccount", "signingIn" or "error" (FriendsPage.phase).
    property string phase: "off"

    // A path for Main.qml's navigate(), like every other page's.
    signal navigateRequested(string target)

    readonly property string errorText: root.controller ? root.controller.error : ""

    EmptyState {
        anchors.fill: parent
        upperThird: true
        title: root.phase === "off" ? qsTr("Friends are off")
             : root.phase === "signingIn" ? qsTr("Connecting…")
             : qsTr("Friends are not connected")
        body: root.phase === "off" ? qsTr("See which server your friends are playing on. There is no chat. While friends are off, Kite sends nothing.")
            : root.phase === "signingIn" ? ""
            : root.errorText.length > 0 ? root.errorText
            : root.phase === "noAccount" ? qsTr("Sign in with a Microsoft account to use friends.")
            : qsTr("Friends could not connect.")
        actionText: root.phase === "off" ? qsTr("Turn on")
                  : root.phase === "noAccount" ? qsTr("Sign in")
                  : root.phase === "error" ? qsTr("Try again") : ""
        onActionTriggered: {
            if (root.phase === "off")
                optIn.open()
            else if (root.phase === "noAccount")
                root.navigateRequested("accounts")
            else if (root.controller)
                root.controller.refresh()
        }

        MeshIcon { iconName: "users"; size: Theme.icon.lg + Theme.space.md; color: Theme.palette.textTertiary }
    }

    FriendsOptInDialog {
        id: optIn
        controller: root.controller
    }
}
