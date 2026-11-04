// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What can be done with one message, from its ⋯: copy its text, send a failed one again or drop it, report one a
 * friend sent. One lives in the conversation and every message opens it; it only says what was asked for.
 */
Menu {
    id: root

    property string key
    property string messageId
    property string text
    property bool mine: false
    property bool deleted: false
    property bool failed: false
    // Where the ⋯ that opened it ends, in this menu's parent: the menu hangs from there, right edges in line.
    property real anchorRight: 0
    property real anchorBottom: 0

    signal copyRequested(string text)
    signal reportRequested(string messageId, string text)
    signal retryRequested(string key)
    signal discardRequested(string key)

    // @p message: a FriendChatMessage.
    function openFor(button, message) {
        root.key = message.key
        root.messageId = message.messageId
        root.text = message.text
        root.mine = message.mine
        root.deleted = message.deleted
        root.failed = message.sendState === "failed"
        var at = button.mapToItem(root.parent, button.width, button.height)
        root.anchorRight = at.x
        root.anchorBottom = at.y
        root.open()
    }

    x: root.anchorRight - root.width
    y: root.anchorBottom + Theme.space.xs

    MenuEntry {
        visible: !root.deleted
        text: qsTr("Copy text")
        icon.source: Icons.url("copy")
        onTriggered: root.copyRequested(root.text)
    }
    MenuEntry {
        visible: root.failed
        text: qsTr("Try again")
        icon.source: Icons.url("refresh")
        onTriggered: root.retryRequested(root.key)
    }
    MenuEntry {
        visible: root.failed
        text: qsTr("Delete")
        icon.source: Icons.url("trash")
        onTriggered: root.discardRequested(root.key)
    }
    // Only what a friend sent, once the server holds it: that is what a report can point at.
    MenuEntry {
        visible: !root.mine && !root.deleted && root.messageId.length > 0
        text: qsTr("Report…")
        icon.source: Icons.url("alert-triangle")
        onTriggered: root.reportRequested(root.messageId, root.text)
    }
}
