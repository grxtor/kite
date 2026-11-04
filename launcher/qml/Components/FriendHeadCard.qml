// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The card a click on a head in the friends strip opens, a profile popout like Discord's: the head bigger with the
 * name and a word on where they are, then where they play (FriendPlaceBlock: the server's live favicon, players,
 * version, Join) and a Message field with the unread count (chat is coming). ⋯ holds Remove and Block. One card for
 * the strip, since a card in a head would close under the pointer when the lists are rebuilt. A click on the strip
 * does not close it (the strip decides): a press that closed it first would reopen it on that head.
 */
Popup {
    id: root

    // FriendsController, or null.
    property var controller: null
    // The panel's FriendPersonMenu.
    property var menu: null
    property var joinFlow: null
    // Main.qml's FriendPlaces and FriendsState.invites.
    property FriendPlaces places: FriendPlaces {}
    property var invites: []
    property bool stale: false
    property bool actionsEnabled: true
    property string friendUuid

    readonly property var friend: root.find(root.controller ? root.controller.friends : [], root.friendUuid)
    readonly property bool known: root.friend.uuid !== undefined
    readonly property var presence: Format.friendPresence(root.friend)

    readonly property int cardWidth: 296
    // One scale for heads: 32 in rows, 40 in the strip, 48 here.
    readonly property int headSize: 48

    signal joinRequested(var friend)

    function find(list, uuid) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].uuid === uuid)
                return list[i]
        }
        return ({})
    }

    // The head's centre in the strip's coordinates; the card stays centred on it as it grows.
    property real centreY: 0

    // Opens beside @p head (a FriendsStripHead); the popup keeps itself inside the window (`margins`).
    function openFor(head, uuid) {
        root.friendUuid = uuid
        root.centreY = head.mapToItem(root.parent, 0, head.height / 2).y
        root.open()
    }

    onKnownChanged: if (!root.known && root.opened) root.close()

    x: -root.width - Theme.space.sm
    y: Math.round(root.centreY - root.height / 2)
    width: root.cardWidth
    // Compact for a few lines, like Discord's popout: menus' glass, a smaller corner and padding than a dialog's.
    padding: Theme.space.md
    margins: Theme.space.sm
    background: GlassSurface {
        radius: Theme.radius.md
        level: Theme.elevation.md
    }
    // Esc only reaches a popup that has the focus.
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    Connections {
        target: root.menu
        ignoreUnknownSignals: true
        function onRemoveRequested() { root.close() }
        function onBlockRequested() { root.close() }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            FriendHead {
                Layout.alignment: Qt.AlignVCenter
                size: root.headSize
                face: root.known ? root.friend.face : ""
                presence: !root.known ? ""
                        : root.stale && root.friend.state !== "offline" ? "" : root.friend.state
                ringColor: Theme.glass.popup
            }
            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xxs

                Copy {
                    width: parent.width
                    text: root.known ? root.friend.name : ""
                    elide: Text.ElideRight
                    role: "title"
                }
                Copy {
                    width: parent.width
                    text: root.known ? root.presence.text : ""
                    elide: Text.ElideRight
                    role: "label"
                    // A list that could not be updated says nothing for sure.
                    tone: root.stale ? "tertiary" : root.presence.tone
                }
            }
            FriendMoreButton {
                Layout.alignment: Qt.AlignTop
                menu: root.menu
                personUuid: root.friendUuid
                personName: root.known ? root.friend.name : ""
                enabled: root.actionsEnabled
            }
        }

        FriendPlaceBlock {
            Layout.fillWidth: true
            friend: root.friend
            place: root.places.of(root.friend)
            invite: root.invites.find((one) => one.uuid === root.friendUuid) || null
            joinFlow: root.joinFlow
            actionsEnabled: root.actionsEnabled
            onJoinRequested: {
                root.close()
                root.joinRequested(root.friend)
            }
        }

        FriendMessageField {
            Layout.fillWidth: true
            name: root.known ? root.friend.name : ""
            unread: root.friend.unread || 0
        }
    }
}
