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
 * are behind the row's ⋯ menu.
 */
Item {
    id: root

    // InstanceDetails.servers (ServersListModel) + serversDir.
    property var model: null
    property string serversDir: ""
    readonly property bool unlocked: !!root.model && !root.model.locked
    readonly property int count: list.count
    readonly property string lockedReason: qsTr("The game is running. The server list can be changed once it has closed.")
    signal openFolderRequested(string path)
    signal joinRequested(string address)

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

                width: list.width
                title: row.name.length > 0 ? row.name : qsTr("Minecraft Server")
                meta: row.address.length > 0 ? row.address : qsTr("No address yet")
                thumb.source: row.iconUrl
                thumb.glyph: "server"
                onMenuRequested: rowMenu.popup()

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
