// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The big servers to join in one click, side by side like a browser's bookmarks (QuickJoinEntry each), pinged when
 * shown and again every minute while shown. What does not fit moves into a ⋯ menu at the end. A click asks to join
 * with the selected instance (`joinRequested`); a right-click offers another instance or the address.
 */
Item {
    id: root

    // FeaturedServers.servers.
    property var servers: []
    // ServerPinger, or null.
    property var pinger: null
    // FriendsController.friends, or [].
    property var friends: []
    // ServerJoinFlow, which names the instance a click starts.
    property var joinFlow: null

    signal joinRequested(var server)
    signal joinWithRequested(var server)

    readonly property int gap: Theme.space.xxs
    readonly property int pingIntervalMs: 60 * 1000
    // How many entries are shown; the rest wait in the ⋯ menu.
    property int fitCount: root.servers.length
    // Every entry side by side, shown or not, so a bar sized by it does not shrink as entries move to the menu.
    property int fullWidth: 0
    readonly property var hidden: root.servers.slice(root.fitCount)

    implicitHeight: Theme.control.heightSm
    implicitWidth: root.fullWidth

    function statusOf(address, revision) {
        return root.pinger ? root.pinger.status(address) : ({})
    }
    function friendsOn(address) {
        return root.pinger && root.friends.length > 0 ? root.pinger.friendsOn(address, root.friends) : []
    }
    function pingAll() {
        if (!root.pinger || !root.visible)
            return
        for (var i = 0; i < root.servers.length; ++i)
            root.pinger.ping(root.servers[i].address)
    }
    // Entries in order while they fit; the ⋯ button takes its room once one does not.
    function measure() {
        var widths = []
        var total = 0
        for (var i = 0; i < repeater.count; ++i) {
            var entry = repeater.itemAt(i)
            widths.push(entry ? entry.implicitWidth : 0)
            total += widths[i] + (i > 0 ? root.gap : 0)
        }
        root.fullWidth = total
        if (total <= root.width) {
            root.fitCount = widths.length
            return
        }
        var room = root.width - more.implicitWidth - root.gap
        var used = 0
        var count = 0
        while (count < widths.length && used + widths[count] + (count > 0 ? root.gap : 0) <= room) {
            used += widths[count] + (count > 0 ? root.gap : 0)
            ++count
        }
        root.fitCount = count
    }

    onWidthChanged: Qt.callLater(root.measure)
    onServersChanged: {
        Qt.callLater(root.measure)
        root.pingAll()
    }
    onVisibleChanged: root.pingAll()
    Component.onCompleted: root.pingAll()

    Timer {
        interval: root.pingIntervalMs
        repeat: true
        running: root.visible && !!root.pinger
        onTriggered: root.pingAll()
    }

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.gap

        Repeater {
            id: repeater
            model: root.servers
            delegate: QuickJoinEntry {
                id: entry
                required property var modelData
                required property int index

                visible: entry.index < root.fitCount
                server: entry.modelData
                status: root.statusOf(entry.modelData.address, root.pinger ? root.pinger.revision : 0)
                friends: root.friendsOn(entry.modelData.address)
                joinFlow: root.joinFlow
                onClicked: root.joinRequested(entry.modelData)
                onMenuRequested: {
                    serverMenu.server = entry.modelData
                    serverMenu.instanceName = root.joinFlow ? root.joinFlow.instanceName(entry.modelData) : ""
                    serverMenu.popup(entry, 0, entry.height)
                }
                onImplicitWidthChanged: Qt.callLater(root.measure)
            }
        }

        IconButton {
            id: more
            anchors.verticalCenter: parent.verticalCenter
            visible: root.hidden.length > 0
            size: Theme.control.heightSm
            iconName: "more"
            tip: qsTr("More servers")
            onClicked: overflowMenu.popup(more, 0, more.height)
        }
    }

    Menu {
        id: serverMenu
        property var server: ({})
        property string instanceName: ""

        MenuItem {
            text: serverMenu.instanceName.length > 0 ? qsTr("Join with %1").arg(serverMenu.instanceName) : qsTr("Join…")
            icon.source: Icons.url("play")
            onTriggered: root.joinRequested(serverMenu.server)
        }
        MenuItem {
            text: qsTr("Join with another instance…")
            icon.source: Icons.url("layers")
            onTriggered: root.joinWithRequested(serverMenu.server)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Copy address")
            icon.source: Icons.url("copy")
            onTriggered: clipboard.copyText(serverMenu.server.address)
        }
    }

    Menu {
        id: overflowMenu

        Instantiator {
            model: root.hidden
            delegate: MenuItem {
                required property var modelData
                readonly property var status: root.statusOf(modelData.address, root.pinger ? root.pinger.revision : 0)

                text: status.state === "online" && status.online >= 0
                      ? qsTr("%1 · %2 online").arg(modelData.name)
                                              .arg(Number(status.online).toLocaleString(Qt.locale(), "f", 0))
                      : modelData.name
                // A favicon keeps its own colours.
                icon.source: status.favicon || Icons.url("server")
                icon.color: status.favicon ? "transparent" : Theme.palette.textPrimary
                onTriggered: root.joinRequested(modelData)
            }
            onObjectAdded: (index, object) => overflowMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => overflowMenu.removeItem(object)
        }
    }

    ClipboardText { id: clipboard }
}
