// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The question before friends are switched on, from Home's side panel and
 * from Settings: plainly what is shared, and that nothing is sent while it
 * stays off. Confirming turns it on (FriendsController.enabled).
 */
ConfirmDialog {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    title: qsTr("Turn on friends?")
    confirmText: qsTr("Turn on")
    danger: false
    text: qsTr("Your friends will see your Minecraft name, whether you are online, the instance and Minecraft version you play, and the name of the server you are on. A server reached by a raw IP address, or on your home network, is never named.")
          + "\n\n" + qsTr("You can go invisible or hide the server whenever you like in Settings. While this is off, Kite sends nothing.")

    onConfirmed: if (root.controller) root.controller.enabled = true
}
