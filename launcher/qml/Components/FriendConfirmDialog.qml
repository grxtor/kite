// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Removing a friend and blocking someone, both after a question: what a
 * person's ⋯ menu (FriendPersonMenu) asks for ends here. The Friends page and
 * the friends panel on the window's right edge each hold one, so neither has
 * to load the other to ask. `ask()` words the question; on yes, the
 * controller acts, and what the server answered comes back as a toast from
 * Main.qml, off FriendsController.actionFinished.
 */
ConfirmDialog {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    property string mode: "remove"
    property string targetUuid
    property string targetName
    readonly property bool removing: root.mode === "remove"

    // @p mode: "remove" a friend, "block" a friend, or "blockRequest" (the
    // person is not a friend: their request is what goes).
    function ask(mode, uuid, name) {
        root.mode = mode
        root.targetUuid = uuid
        root.targetName = name
        root.open()
    }

    title: root.removing ? qsTr("Remove %1 from friends?").arg(root.targetName) : qsTr("Block %1?").arg(root.targetName)
    confirmText: root.removing ? qsTr("Remove friend") : qsTr("Block")
    text: root.removing
          ? qsTr("%1 will no longer see when you are online, and you will not see them. You can add each other again later.").arg(root.targetName)
          : root.mode === "blockRequest"
            ? qsTr("Their request is declined, and %1 can no longer send you requests or see when you are online. They are not told.").arg(root.targetName)
            : qsTr("%1 will be taken off your friends and can no longer send you requests or see when you are online. They are not told.").arg(root.targetName)
    onConfirmed: {
        if (root.removing)
            root.controller.removeFriend(root.targetUuid)
        else
            root.controller.block(root.targetUuid)
    }
}
