// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * What can be done to a person on the Friends page: message them, invite a friend to the server you are on (or
 * take the invite back), remove them from your friends, or block them (a non-friend can only be blocked). One lives
 * on the page and every row's ⋯ opens it: a list read again while a menu is open rebuilds its rows, and a menu in
 * one would vanish. It only says what was asked for; the page confirms and acts.
 */
Menu {
    id: root

    property string personUuid
    property string personName
    property bool canRemove: true
    property bool canMessage: true
    // FriendsController (inviteTarget, outgoingInvites, sendInvite(), cancelInvite()) and FriendPlaces, for invites.
    property var controller: null
    property FriendPlaces places: FriendPlaces {}

    // The server you are on as FriendPlaces reads a friend's, or null while you are not on one.
    readonly property var here: root.controller && root.controller.inviteTarget && root.controller.inviteTarget.host
                                ? { state: "playing", worldType: "server", host: root.controller.inviteTarget.host,
                                    port: root.controller.inviteTarget.port } : null
    readonly property var sentInvite: root.controller
                                      ? root.controller.outgoingInvites.find((one) => one.uuid === root.personUuid) || null
                                      : null
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
        visible: root.canRemove && !!root.here
        text: qsTr("Invite to %1").arg(root.here ? root.places.of(root.here).name : "")
        icon.source: Icons.url("send")
        onTriggered: root.controller.sendInvite(root.personUuid, Format.friendAddress(root.here))
    }
    MenuEntry {
        visible: root.canRemove && !!root.sentInvite
        text: qsTr("Take back invite")
        icon.source: Icons.url("undo")
        onTriggered: root.controller.cancelInvite(root.personUuid)
    }
    MenuDivider {
        visible: root.canRemove && (!!root.here || !!root.sentInvite)
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
