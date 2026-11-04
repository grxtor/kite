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
    // "Invite a friend…" in a server's menu, shown while `canInvite` (Friends is connected).
    signal inviteRequested(var server)
    property bool canInvite: false

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
    // Join with… / Copy address for @p server, under @p anchor, or at the pointer without one.
    function openServerMenu(server, anchor) {
        serverMenu.server = server
        serverMenu.instanceName = root.joinFlow ? root.joinFlow.instanceName(server) : ""
        if (anchor)
            serverMenu.popup(anchor, 0, anchor.height)
        else
            serverMenu.popup()
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
                menuOpen: serverMenu.visible && serverMenu.server.id === entry.modelData.id
                onClicked: root.joinRequested(entry.modelData)
                onMenuRequested: root.openServerMenu(entry.modelData, entry)
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
            // Opens leftward, its right edge under the button's, so it stays inside the page's column.
            onClicked: overflowMenu.popup(more, more.width - overflowMenu.width, more.height)
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
        MenuEntry {
            visible: root.canInvite
            text: qsTr("Invite a friend…")
            icon.source: Icons.url("user-plus")
            onTriggered: root.inviteRequested(serverMenu.server)
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

        // What the list is, so a single row reads as a menu.
        MenuItem {
            enabled: false
            background: null
            contentItem: Copy {
                text: qsTr("More servers")
                role: "caption"
                tone: "tertiary"
            }
        }

        // The same line as an entry; a click joins, a right-click offers the server's own menu.
        Instantiator {
            model: root.hidden
            delegate: MenuItem {
                id: hiddenRow
                required property var modelData

                text: hiddenRow.modelData.name
                contentItem: Item {
                    implicitWidth: line.implicitWidth
                    implicitHeight: line.implicitHeight

                    QuickJoinSummary {
                        id: line
                        anchors.verticalCenter: parent.verticalCenter
                        server: hiddenRow.modelData
                        status: root.statusOf(hiddenRow.modelData.address, root.pinger ? root.pinger.revision : 0)
                        friends: root.friendsOn(hiddenRow.modelData.address)
                    }
                }
                onTriggered: root.joinRequested(hiddenRow.modelData)

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        overflowMenu.close()
                        root.openServerMenu(hiddenRow.modelData, null)
                    }
                }
            }
            // After the heading.
            onObjectAdded: (index, object) => overflowMenu.insertItem(index + 1, object)
            onObjectRemoved: (index, object) => overflowMenu.removeItem(object)
        }
    }

    ClipboardText { id: clipboard }
}
