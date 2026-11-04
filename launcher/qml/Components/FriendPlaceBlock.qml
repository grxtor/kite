// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * Where a friend is playing, as a block in their card (FriendHeadCard): on a server its live favicon, name, players
 * and the game version, with Join under them; singleplayer, a server they keep to themselves and the menus say so
 * with the version. An invite from them adds how long it lasts. Shows only while they are in a game.
 */
Rectangle {
    id: root

    property var friend: ({})
    // FriendPlaces.of(friend).
    property var place: ({})
    // Their invite to you (FriendsState.inviteFrom()), or null.
    property var invite: null
    property var joinFlow: null
    property bool actionsEnabled: true

    signal joinRequested()

    readonly property bool onServer: !!root.place.address
    readonly property string title: root.onServer ? root.place.name
                                  : root.friend.worldType === "singleplayer" ? qsTr("Singleplayer")
                                  : root.friend.worldType === "server" ? qsTr("On a server")
                                  : qsTr("Playing")
    readonly property string facts: [root.onServer && root.place.online >= 0
                                     ? qsTr("%1 online").arg(Format.playerCount(root.place.online)) : "",
                                     root.friend.version || ""].filter((part) => part.length > 0).join(" · ")
    readonly property int faviconSize: Theme.control.height

    visible: root.friend.state === "playing"
    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.md
    color: Theme.palette.surfaceSunken

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.space.md
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            ServerFavicon {
                Layout.alignment: Qt.AlignVCenter
                size: root.faviconSize
                tile: true
                source: root.place.favicon || ""
                glyph: root.onServer || root.friend.worldType === "server" ? "server"
                     : root.friend.worldType === "singleplayer" ? "cube" : "play"
            }
            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xxs

                Copy {
                    width: parent.width
                    text: root.title
                    elide: Text.ElideMiddle
                    role: "bodyStrong"
                }
                Copy {
                    width: parent.width
                    visible: root.facts.length > 0
                    text: root.facts
                    elide: Text.ElideRight
                    role: "label"
                    tone: "secondary"
                }
                Copy {
                    width: parent.width
                    visible: !!root.invite
                    text: root.invite ? qsTr("Invited you · %1").arg(Format.inviteTimeLeft(root.invite)) : ""
                    elide: Text.ElideRight
                    role: "label"
                    tone: "accent"
                }
            }
        }

        FriendJoinButton {
            Layout.fillWidth: true
            enabled: root.actionsEnabled
            friend: root.friend
            joinFlow: root.joinFlow
            icon.source: Icons.url("play")
            onClicked: root.joinRequested()
        }

        Copy {
            Layout.fillWidth: true
            visible: Format.friendJoinOff(root.friend)
            text: qsTr("%1 doesn't let friends join").arg(root.friend.name)
            wrapMode: Text.Wrap
            role: "label"
            tone: "tertiary"
        }
    }
}
