// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One big server in a line: its live favicon (a server glyph until it answers), name, a dot that says whether it
 * answers with the players online after it ("Offline" when it does not), and the heads of friends playing there.
 * The quick-join bar's entries and its ⋯ menu rows both show it.
 */
Row {
    id: root

    // A FeaturedServers.servers entry.
    property var server: ({})
    // ServerPinger.status(address).
    property var status: ({})
    // The friends playing there (ServerPinger.friendsOn).
    property var friends: []

    readonly property int faviconSize: Theme.icon.mid
    readonly property int dotSize: 6
    readonly property int headSize: 16
    readonly property string serverState: root.status && root.status.state ? root.status.state : ""
    readonly property bool live: root.serverState === "online"
    readonly property string favicon: root.live && root.status.favicon ? root.status.favicon : ""
    readonly property string players: root.live ? Format.playerCount(root.status.online) : ""
    readonly property string countText: root.serverState === "offline" ? qsTr("Offline") : root.players

    spacing: Theme.space.sm

    Item {
        anchors.verticalCenter: parent.verticalCenter
        width: root.faviconSize
        height: root.faviconSize

        Image {
            id: favicon
            anchors.fill: parent
            source: root.favicon
            sourceSize: Qt.size(root.faviconSize * 2, root.faviconSize * 2)
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: status === Image.Ready
        }
        MeshIcon {
            anchors.centerIn: parent
            visible: favicon.status !== Image.Ready
            iconName: "server"
            size: Theme.icon.xs
            color: Theme.palette.textTertiary
        }
    }

    Copy {
        anchors.verticalCenter: parent.verticalCenter
        text: root.server.name || ""
        role: "label"
        font.weight: Font.DemiBold
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.xs + Theme.space.xxs

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: root.dotSize
            height: root.dotSize
            radius: root.dotSize / 2
            color: root.live ? Theme.palette.success : "transparent"
            border.width: root.live ? 0 : 1
            border.color: Theme.palette.textTertiary
        }
        Copy {
            anchors.verticalCenter: parent.verticalCenter
            visible: text.length > 0
            text: root.countText
            role: "caption"
            tone: root.live ? "secondary" : "tertiary"
            font.features: ({ "tnum": 1 })
        }
    }

    FriendHeads {
        anchors.verticalCenter: parent.verticalCenter
        people: root.friends
        size: root.headSize
    }
}
