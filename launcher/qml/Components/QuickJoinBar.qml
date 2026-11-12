// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The servers to join in one click, side by side like a browser's bookmarks (QuickJoinEntry each), pinged when shown
 * and again every minute while shown: the big ones and the player's own, as they arranged them (QuickJoinLayout).
 * Drag an entry to move it; what does not fit moves into the ⋯ menu at the end, which also adds a server and edits
 * the bar (QuickJoinMenus). A click asks to join with the selected instance (`joinRequested`); a right-click offers
 * another instance, the address or taking it off the bar. After an update, a one-time note says so (QuickJoinHint).
 */
Item {
    id: root

    // QuickJoinLayout, or null.
    property var layout: null
    // The entries shown, in order.
    property var servers: root.layout ? root.layout.shown : []
    // ServerPinger, or null.
    property var pinger: null
    // FriendsController.friends, or [].
    property var friends: []
    // ServerJoinFlow, which names the instance a click starts.
    property var joinFlow: null
    // Nothing else asks for attention (onboarding, "What's new"): the one-time note may show.
    property bool hintAllowed: false

    signal joinRequested(var server)
    signal joinWithRequested(var server)
    // "Invite a friend…" in a server's menu, shown while `canInvite` (Friends is connected).
    signal inviteRequested(var server)
    property bool canInvite: false

    readonly property int gap: Theme.space.xxs
    readonly property int pingIntervalMs: 60 * 1000
    readonly property int hintDelayMs: 800
    readonly property int dropMarkWidth: 2
    // How many entries are shown; the rest wait in the ⋯ menu.
    property int fitCount: root.servers.length
    // Every entry side by side and ⋯, shown or not, so a bar sized by it does not shrink as entries move to the menu.
    property int fullWidth: 0
    readonly property var hidden: root.servers.slice(root.fitCount)

    // The entry being dragged, and the place among the others it would land in.
    property string draggingId: ""
    property int dropIndex: -1
    property int dragStart: -1

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
    function openAdd() { root.dialog(addLoader).start() }
    function openEdit() { root.dialog(editLoader).edit() }
    function dialog(loader) {
        loader.active = true
        return loader.item
    }
    // For review snapshots (KITE_QML_ROUTE "quickjoin=…").
    function debugOpen(what) {
        if (what === "edit")
            root.openEdit()
        else if (what === "add")
            root.openAdd()
        else if (what === "hint")
            hint.open()
        else if (what === "menu")
            menus.openMore(more)
    }
    // Entries in order while they fit; ⋯ always keeps its room at the end.
    function measure() {
        var widths = []
        var total = 0
        for (var i = 0; i < repeater.count; ++i) {
            var entry = repeater.itemAt(i)
            widths.push(entry ? entry.implicitWidth : 0)
            total += widths[i] + root.gap
        }
        root.fullWidth = total + more.implicitWidth
        var room = root.width - more.implicitWidth - root.gap
        var used = 0
        var count = 0
        while (count < widths.length && used + widths[count] + (count > 0 ? root.gap : 0) <= room) {
            used += widths[count] + (count > 0 ? root.gap : 0)
            ++count
        }
        root.fitCount = count
    }

    // Drag: the others' order with the dragged one taken out, and where the pointer (bar x) falls among them.
    function beginDrag(id, index) {
        root.draggingId = id
        root.dragStart = index
        root.dropIndex = index
    }
    function dragTo(x) {
        var to = 0
        for (var i = 0; i < root.fitCount; ++i) {
            var entry = repeater.itemAt(i)
            if (entry && entry.server.id !== root.draggingId && entry.x + entry.width / 2 < x)
                ++to
        }
        root.dropIndex = to
    }
    function endDrag() {
        if (root.layout && root.draggingId.length > 0 && root.dropIndex >= 0 && root.dropIndex !== root.dragStart)
            root.layout.move(root.draggingId, root.dropIndex)
        root.draggingId = ""
        root.dropIndex = -1
        root.dragStart = -1
    }
    // Where the drop mark stands: before the dropIndex-th of the others, or after the last.
    function dropMarkX() {
        var seen = 0
        var lastRight = 0
        for (var i = 0; i < root.fitCount; ++i) {
            var entry = repeater.itemAt(i)
            if (!entry || entry.server.id === root.draggingId)
                continue
            if (seen === root.dropIndex)
                return entry.x - root.gap / 2 - root.dropMarkWidth / 2
            lastRight = entry.x + entry.width
            ++seen
        }
        return lastRight + root.gap / 2 - root.dropMarkWidth / 2
    }

    onWidthChanged: Qt.callLater(root.measure)
    onServersChanged: {
        Qt.callLater(root.measure)
        root.pingAll()
    }
    onVisibleChanged: {
        root.pingAll()
        if (!root.visible && hint.opened)
            hint.close()
    }
    Component.onCompleted: root.pingAll()

    Timer {
        interval: root.pingIntervalMs
        repeat: true
        running: root.visible && !!root.pinger
        onTriggered: root.pingAll()
    }

    // The note waits for the bar to settle, and for nothing else to be asking.
    Timer {
        interval: root.hintDelayMs
        running: root.visible && root.hintAllowed && !!root.layout && !root.layout.hintSeen
                 && root.servers.length > 0 && !hint.opened
        onTriggered: hint.open()
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
                opacity: root.draggingId === entry.modelData.id ? Theme.opacity.disabled : 1
                server: entry.modelData
                status: root.statusOf(entry.modelData.address, root.pinger ? root.pinger.revision : 0)
                friends: root.friendsOn(entry.modelData.address)
                joinFlow: root.joinFlow
                menuOpen: root.draggingId.length > 0
                          || (menus.serverMenuOpen && menus.serverMenuId === entry.modelData.id)
                onClicked: root.joinRequested(entry.modelData)
                onMenuRequested: menus.openServer(entry.modelData, entry)
                onImplicitWidthChanged: Qt.callLater(root.measure)

                // Sideways only; a click that does not travel stays a click.
                DragHandler {
                    target: null
                    enabled: !!root.layout
                    yAxis.enabled: false
                    onActiveChanged: active ? root.beginDrag(entry.modelData.id, entry.index) : root.endDrag()
                    onCentroidChanged: if (active) root.dragTo(entry.mapToItem(row, centroid.position.x, 0).x)
                }
            }
        }

        IconButton {
            id: more
            objectName: "quickJoinMore"
            anchors.verticalCenter: parent.verticalCenter
            size: Theme.control.heightSm
            iconName: "more"
            tip: root.hidden.length > 0 ? qsTr("More servers") : qsTr("More")
            onClicked: menus.openMore(more)
        }
    }

    Rectangle {
        objectName: "quickJoinDropMark"
        visible: root.draggingId.length > 0 && root.dropIndex >= 0 && root.dropIndex !== root.dragStart
        x: row.x + root.dropMarkX()
        anchors.verticalCenter: parent.verticalCenter
        width: root.dropMarkWidth
        height: root.height - Theme.space.sm
        radius: width / 2
        color: Theme.palette.accent
    }

    QuickJoinMenus {
        id: menus
        bar: root
    }

    QuickJoinHint {
        id: hint
        objectName: "quickJoinHint"
        layout: root.layout
    }

    Loader {
        id: addLoader
        active: false
        sourceComponent: QuickJoinAddDialog {
            layout: root.layout
            pinger: root.pinger
        }
    }

    Loader {
        id: editLoader
        active: false
        sourceComponent: QuickJoinEditDialog {
            layout: root.layout
            pinger: root.pinger
        }
    }
}
