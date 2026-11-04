// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A conversation with one friend in the friends panel, like a Discord DM: the friend on top (FriendChatHeader),
 * the messages (FriendChatList), the field to write in (FriendChatComposer). It shows whatever FriendsChat has
 * open; Back or Esc closes it. The head or name in the header opens the friend's card (FriendHeadCard, as the strip's
 * heads do). Someone removed from the friends list keeps the history on screen, without the field. ⋯ on a message
 * copies, retries or reports it (FriendReportDialog).
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    // The panel's FriendPersonMenu: Remove and Block from the header's ⋯.
    property var menu: null
    property string myFace
    property bool connected: true
    property bool stale: false
    // Main.qml's FriendPlaces and FriendJoinFlow, and FriendsState.invites, for the header and the card.
    property FriendPlaces places: FriendPlaces {}
    property var joinFlow: null
    property var invites: []

    signal joinRequested(var friend)

    readonly property var chat: root.controller ? root.controller.chat : null
    readonly property string uuid: root.chat ? root.chat.openUuid : ""
    readonly property var listed: root.find(root.controller ? root.controller.friends : [], root.uuid)
    readonly property bool isFriend: root.listed.uuid !== undefined
    // The friend as last seen on the list, so a removed one keeps their name and head here.
    property var lastKnown: ({})
    readonly property var friend: root.isFriend ? root.listed
                                : root.lastKnown.uuid === root.uuid ? root.lastKnown : ({})

    function find(list, uuid) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].uuid === uuid)
                return list[i]
        }
        return ({})
    }

    // Not through isFriend: a handler may run before the bindings that hang on the same change.
    onListedChanged: if (root.listed.uuid !== undefined) root.lastKnown = root.listed
    onUuidChanged: {
        card.close()
        if (root.uuid.length > 0)
            Qt.callLater(composer.focusField)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.canvas
    }

    FriendChatHeader {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        friend: root.friend
        known: root.isFriend
        menu: root.menu
        place: root.places.of(root.friend)
        stale: root.stale
        actionsEnabled: root.connected
        cardOpen: card.opened
        onBackRequested: root.chat.close()
        onProfileRequested: (head) => {
            if (card.opened)
                card.close()
            else
                card.openFor(head, root.uuid)
        }
    }

    FriendChatList {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: composer.top
        chat: root.chat
        friend: root.friend
        myName: root.controller ? root.controller.myName : ""
        myFace: root.myFace
        menu: messageMenu
    }

    FriendChatComposer {
        id: composer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        chat: root.chat
        name: root.friend.name || ""
        canSend: root.isFriend && root.connected
        reason: !root.isFriend ? qsTr("%1 is not on your friends list any more, so you can't message them.").arg(root.friend.name || "")
                               : qsTr("Messages can't be sent until Kite's friends server answers.")
        onCloseRequested: root.chat.close()
    }

    // Beside the panel, level with the header's head; a press outside the header closes it. Its Message field only
    // closes it, as this is that conversation.
    FriendHeadCard {
        id: card
        objectName: "chatProfileCard"
        parent: header
        controller: root.controller
        places: root.places
        invites: root.invites
        menu: root.menu
        joinFlow: root.joinFlow
        stale: root.stale
        actionsEnabled: root.connected
        onJoinRequested: (friend) => root.joinRequested(friend)
    }

    ClipboardText { id: clipboard }

    FriendChatMessageMenu {
        id: messageMenu
        objectName: "chatMessageMenu"
        onCopyRequested: (text) => clipboard.copyText(text)
        onRetryRequested: (key) => root.chat.retry(key)
        onDiscardRequested: (key) => root.chat.discard(key)
        onReportRequested: (messageId, text) => report.ask(root.uuid, root.friend.name || "", messageId, text)
    }

    FriendReportDialog {
        id: report
        objectName: "chatReportDialog"
        controller: root.controller
    }
}
