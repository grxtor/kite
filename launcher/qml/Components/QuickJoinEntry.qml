// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One big server in the quick-join bar: its live favicon, name, players online behind a dot that says whether it
 * answers, and the heads of friends playing there. A click joins (`clicked`); a right-click asks for the server's
 * menu (`menuRequested`). Flat until hovered, like a bookmark in a browser's bar.
 */
AbstractButton {
    id: root

    // A FeaturedServers.servers entry: { id, name, address, note, minVersion, maxVersion }.
    property var server: ({})
    // ServerPinger.status(address).
    property var status: ({})
    // The friends playing there (ServerPinger.friendsOn).
    property var friends: []
    // ServerJoinFlow, which names the instance a click starts (asked as the pointer arrives, so never stale).
    property var joinFlow: null

    signal menuRequested()

    readonly property int faviconSize: Theme.icon.mid
    readonly property int dotSize: 6
    readonly property int headSize: 16
    readonly property string serverState: root.status && root.status.state ? root.status.state : ""
    readonly property bool live: root.serverState === "online"
    readonly property string favicon: root.live && root.status.favicon ? root.status.favicon : ""
    readonly property string players: root.live ? Format.playerCount(root.status.online) : ""
    readonly property string countText: root.serverState === "offline" ? qsTr("Offline") : root.players

    function tipText() {
        var instanceName = root.joinFlow ? root.joinFlow.instanceName(root.server) : ""
        var lines = [instanceName.length > 0 ? qsTr("Join %1 with %2").arg(root.server.name).arg(instanceName)
                                             : qsTr("Join %1").arg(root.server.name)]
        if (root.live) {
            var facts = []
            if (root.players.length > 0)
                facts.push(qsTr("%1 online").arg(root.players))
            if (root.status.ping >= 0)
                facts.push(qsTr("%1 ms").arg(root.status.ping))
            if (facts.length > 0)
                lines.push(facts.join(" · "))
        } else if (root.serverState === "offline") {
            lines.push(qsTr("Not answering right now"))
        }
        if (root.friends.length > 0)
            lines.push(Format.friendsHere(root.friends))
        return lines.join("\n")
    }

    implicitHeight: Theme.control.heightSm
    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
    leftPadding: Theme.space.xs + Theme.space.xxs
    rightPadding: Theme.space.sm + Theme.space.xxs
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: root.server.name || ""
    Accessible.description: root.players.length > 0 ? qsTr("%1 online").arg(root.players) : root.countText

    ToolTip.visible: root.hovered
    ToolTip.delay: 500
    ToolTip.text: root.hovered ? root.tipText() : ""

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: root.menuRequested()
    }

    background: StateFill {
        radius: Theme.radius.sm
        hovered: root.hovered
        down: root.down

        FocusFrame { around: 0; visible: root.visualFocus }
    }

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.verticalCenter: parent.verticalCenter
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
    }
}
