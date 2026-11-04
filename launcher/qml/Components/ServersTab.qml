// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * This instance's servers.dat as the game's own server list: one read-only row
 * per server with "Join" to launch straight into it; edit, reorder and remove
 * are behind the row's ⋯ menu. Each row is pinged while the tab is shown: its
 * message, players online, ping, the friends playing there, and its live
 * favicon when the game saved none.
 */
Item {
    id: root

    // InstanceDetails.servers (ServersListModel) + serversDir.
    property var model: null
    property string serversDir: ""
    // ServerPinger, or null; FriendsController.friends, or [].
    property var pinger: null
    property var friends: []
    readonly property bool unlocked: !!root.model && !root.model.locked
    readonly property int count: list.count
    readonly property string lockedReason: qsTr("The game is running. The server list can be changed once it has closed.")
    signal openFolderRequested(string path)
    signal joinRequested(string address)
    // Every row asks its server again (the Refresh button).
    signal refreshRequested()

    // The lines of a server's message the game shows (ServerStatus::kMotdLines).
    readonly property int motdLines: 2
    readonly property int headSize: 24
    readonly property int liveWidth: 112

    function statusOf(address, revision) {
        return root.pinger && address.length > 0 ? root.pinger.status(address) : ({})
    }
    function ping(address) {
        if (root.pinger && root.visible && address.trim().length > 0)
            root.pinger.ping(address)
    }

    readonly property var packChoices: [
        { value: 0, label: qsTr("Ask") },
        { value: 1, label: qsTr("Always") },
        { value: 2, label: qsTr("Never") }
    ]

    function addServer() {
        serverDialog.row = -1
        serverDialog.open()
    }
    function editServer(row, name, address, acceptTextures) {
        serverDialog.row = row
        serverDialog.name = name
        serverDialog.address = address
        serverDialog.acceptTextures = acceptTextures
        serverDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        TabToolbar {
            Layout.fillWidth: true

            IconButton {
                flat: false
                visible: !!root.pinger
                iconName: "refresh"
                tip: qsTr("Refresh")
                onClicked: root.refreshRequested()
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.serversDir)
            }
            HoverReason {
                reason: root.lockedReason
                Button {
                    enabled: root.unlocked
                    text: qsTr("Add server")
                    icon.source: Icons.url("plus")
                    onClicked: root.addServer()
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !root.unlocked && !!root.model
            text: root.lockedReason
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            ScrollBar.vertical: ScrollBar {}

            delegate: TabListRow {
                id: row
                required property int index
                required property string name
                required property string address
                required property int acceptTextures
                // The favicon the game saved when it last pinged the server, as a data: URL; "" if
                // never.
                required property string iconUrl
                readonly property var live: root.statusOf(row.address, root.pinger ? root.pinger.revision : 0)
                readonly property bool online: row.live.state === "online"
                readonly property bool hasMotd: row.online && row.live.motd.length > 0
                readonly property var here: root.pinger && root.friends.length > 0
                                            ? root.pinger.friendsOn(row.address, root.friends) : []
                readonly property string players: row.online ? Format.playerCount(row.live.online) : ""

                width: list.width
                title: row.name.length > 0 ? row.name : qsTr("Minecraft Server")
                // The server's own words once it answers; its address until then.
                meta: row.hasMotd ? (Theme.dark ? row.live.motd : row.live.motdLight)
                    : row.address.length > 0 ? row.address : qsTr("No address yet")
                // The message is escaped StyledText (ServerStatus::formatMotd); the address is plain.
                metaFormat: row.hasMotd ? Text.StyledText : Text.PlainText
                metaLines: row.hasMotd ? root.motdLines : 1
                thumb.source: row.iconUrl.length > 0 ? row.iconUrl : row.online ? row.live.favicon : ""
                thumb.glyph: "server"
                onMenuRequested: rowMenu.popup()
                onAddressChanged: root.ping(row.address)
                Component.onCompleted: root.ping(row.address)

                Connections {
                    target: root
                    function onVisibleChanged() { root.ping(row.address) }
                    function onRefreshRequested() {
                        if (root.pinger && row.address.trim().length > 0)
                            root.pinger.refresh(row.address)
                    }
                }

                Row {
                    id: friendsHere
                    Layout.alignment: Qt.AlignVCenter
                    visible: row.here.length > 0
                    spacing: Theme.space.sm

                    FriendHeads {
                        anchors.verticalCenter: parent.verticalCenter
                        people: row.here
                        size: root.headSize
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.here.length === 1 ? qsTr("1 friend here") : qsTr("%1 friends here").arg(row.here.length)
                        role: "label"
                        tone: "secondary"
                    }
                    HoverHandler { id: friendsHover }
                    ToolTip.visible: friendsHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: Format.friendsHere(row.here)
                }

                // Players and ping, or quietly that the server does not answer; nothing while it is first asked.
                Column {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: root.liveWidth
                    visible: !!root.pinger
                    spacing: Theme.space.xxs

                    Copy {
                        anchors.right: parent.right
                        visible: text.length > 0
                        text: row.live.state === "offline" ? qsTr("Can't reach")
                            : row.players.length > 0 ? qsTr("%1 online").arg(row.players) : ""
                        role: "label"
                        tone: row.online ? "secondary" : "tertiary"
                        font.features: ({ "tnum": 1 })
                    }
                    Copy {
                        anchors.right: parent.right
                        visible: row.online && row.live.ping >= 0
                        text: qsTr("%1 ms").arg(row.live.ping)
                        role: "caption"
                        tone: "tertiary"
                        font.features: ({ "tnum": 1 })
                    }
                }

                Button {
                    text: qsTr("Join")
                    icon.source: Icons.url("play")
                    enabled: row.address.trim().length > 0
                    onClicked: root.joinRequested(row.address)
                }

                IconButton {
                    id: moreButton
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)
                }

                Menu {
                    id: rowMenu
                    MenuItem {
                        enabled: root.unlocked
                        text: qsTr("Edit…")
                        icon.source: Icons.url("edit")
                        onTriggered: root.editServer(row.index, row.name, row.address, row.acceptTextures)
                    }
                    MenuItem {
                        enabled: root.unlocked && row.index > 0
                        text: qsTr("Move up")
                        icon.source: Icons.url("arrow-up")
                        onTriggered: if (root.model) root.model.moveUp(row.index)
                    }
                    MenuItem {
                        enabled: root.unlocked && row.index < list.count - 1
                        text: qsTr("Move down")
                        icon.source: Icons.url("arrow-down")
                        onTriggered: if (root.model) root.model.moveDown(row.index)
                    }
                    MenuSeparator {}
                    MenuItem {
                        enabled: root.unlocked
                        text: qsTr("Remove…")
                        icon.source: Icons.url("trash")
                        onTriggered: {
                            confirm.row = row.index
                            confirm.text = qsTr("Remove “%1” from this instance's server list?").arg(row.name.length > 0 ? row.name : row.address)
                            confirm.open()
                        }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: list
    }

    AppDialog {
        id: serverDialog
        property int row: -1
        property string name
        property string address
        property int acceptTextures: 0
        property int packChoice: 0

        size: "sm"
        title: row < 0 ? qsTr("Add server") : qsTr("Edit server")
        primaryText: row < 0 ? qsTr("Add server") : qsTr("Save")
        primaryEnabled: addressField.text.trim().length > 0
        // save() closes the dialog itself, and stays open when there is nothing to save.
        closeOnPrimary: false
        onPrimaryClicked: save()

        onOpened: {
            if (row < 0) {
                name = ""
                address = ""
                acceptTextures = 0
            }
            nameField.text = name
            addressField.text = address
            packChoice = acceptTextures
            nameField.forceActiveFocus()
            nameField.selectAll()
        }

        function save() {
            if (!root.model || addressField.text.trim().length === 0)
                return
            var target = row
            if (target < 0) {
                target = root.model.addServer()
                if (target < 0)
                    return
            }
            root.model.setName(target, nameField.text.trim())
            root.model.setAddress(target, addressField.text.trim())
            root.model.setAcceptTextures(target, packChoice)
            list.positionViewAtIndex(target, ListView.Contain)
            close()
        }

        contentItem: Column {
            spacing: Theme.space.sm

            FieldLabel { text: qsTr("Name") }
            TextField {
                id: nameField
                width: parent.width
                placeholderText: qsTr("Minecraft Server")
                selectByMouse: true
                onAccepted: addressField.forceActiveFocus()
            }

            Item { width: 1; height: Theme.space.xs }
            FieldLabel { text: qsTr("Address") }
            TextField {
                id: addressField
                width: parent.width
                placeholderText: qsTr("play.example.com")
                selectByMouse: true
                onAccepted: serverDialog.save()
            }

            Item { width: 1; height: Theme.space.xs }
            FieldLabel { text: qsTr("Server resource packs") }
            Row {
                spacing: Theme.space.lg
                Repeater {
                    model: root.packChoices
                    delegate: RadioButton {
                        required property var modelData
                        text: modelData.label
                        // The dialog owns the choice, so a click sets it and puts the binding back.
                        autoExclusive: false
                        checked: serverDialog.packChoice === modelData.value
                        onClicked: {
                            serverDialog.packChoice = modelData.value
                            checked = Qt.binding(() => serverDialog.packChoice === modelData.value)
                        }
                    }
                }
            }
        }

    }

    ConfirmDialog {
        id: confirm
        property int row: -1
        title: qsTr("Remove server")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.model) root.model.removeServer(row)
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No servers yet")
        body: qsTr("Servers you add in game show up here. You can also add one yourself.")
        MeshIcon { iconName: "server"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }
}
