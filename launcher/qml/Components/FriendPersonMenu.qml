// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What can be done to a person on the Friends page: message them, remove them from your friends, or block them (a
 * non-friend can only be blocked). One lives on the page and every row's ⋯ opens it: a list read again while a menu
 * is open rebuilds its rows, and a menu in one would vanish. It only says what was asked for; the page confirms and
 * acts.
 */
Menu {
    id: root

    property string personUuid
    property string personName
    property bool canRemove: true
    property bool canMessage: true
    // Where the ⋯ that opened it ends, in this menu's parent: the menu hangs from there, right edges in line. Its
    // width settles once its entries are laid out, so its x follows it rather than being set once.
    property real anchorRight: 0
    property real anchorBottom: 0

    signal messageRequested(string uuid, string name)
    signal removeRequested(string uuid, string name)
    // `friend`: they are on your list, so blocking also removes them.
    signal blockRequested(string uuid, string name, bool friend)

    // @p messagable: whether Message shows; as @p removable (a friend) when not given.
    function openFor(button, uuid, name, removable, messagable) {
        root.personUuid = uuid
        root.personName = name
        root.canRemove = removable
        root.canMessage = messagable === undefined ? removable : messagable
        var at = button.mapToItem(root.parent, button.width, button.height)
        root.anchorRight = at.x
        root.anchorBottom = at.y
        root.open()
    }

    x: root.anchorRight - root.width
    y: root.anchorBottom + Theme.space.xs

    MenuEntry {
        visible: root.canMessage
        text: qsTr("Message")
        icon.source: Icons.url("message-circle")
        onTriggered: root.messageRequested(root.personUuid, root.personName)
    }
    MenuEntry {
        visible: root.canRemove
        text: qsTr("Remove friend…")
        icon.source: Icons.url("x")
        onTriggered: root.removeRequested(root.personUuid, root.personName)
    }
    MenuItem {
        text: qsTr("Block…")
        icon.source: Icons.url("ban")
        onTriggered: root.blockRequested(root.personUuid, root.personName, root.canRemove)
    }
}
