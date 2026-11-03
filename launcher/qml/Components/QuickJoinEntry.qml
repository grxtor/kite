// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One big server in the quick-join bar (QuickJoinSummary: favicon, name, players online behind a dot, friends'
 * heads). A click joins (`clicked`); a right-click asks for the server's menu (`menuRequested`). Flat until
 * hovered, like a bookmark in a browser's bar.
 */
AbstractButton {
    id: root

    // A FeaturedServers.servers entry: { id, name, address, note, minVersion, maxVersion }.
    property var server: ({})
    // ServerPinger.status(address).
    property var status: ({})
    // The friends playing there (ServerPinger.friendsOn).
    property var friends: []
    // ServerJoinFlow, which names the instance a click starts (the tooltip follows the selection).
    property var joinFlow: null

    // Its menu is open: the tooltip keeps out of the way.
    property bool menuOpen: false

    signal menuRequested()

    readonly property bool live: summary.live
    readonly property string players: summary.players

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
        } else if (summary.serverState === "offline") {
            lines.push(qsTr("Not answering right now"))
        }
        if (root.friends.length > 0)
            lines.push(Format.friendsHere(root.friends))
        return lines.join("\n")
    }

    implicitHeight: Theme.control.heightSm
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    leftPadding: Theme.space.xs + Theme.space.xxs
    rightPadding: Theme.space.sm + Theme.space.xxs
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: root.server.name || ""
    Accessible.description: root.players.length > 0 ? qsTr("%1 online").arg(root.players) : summary.countText

    ToolTip.visible: root.hovered && !root.menuOpen
    ToolTip.delay: 500
    // Not tied to `hovered`: the shared tooltip takes its text as it is told to show, which may come first.
    ToolTip.text: root.tipText()

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
        implicitWidth: summary.implicitWidth
        implicitHeight: summary.implicitHeight

        QuickJoinSummary {
            id: summary
            anchors.verticalCenter: parent.verticalCenter
            server: root.server
            status: root.status
            friends: root.friends
        }
    }
}
