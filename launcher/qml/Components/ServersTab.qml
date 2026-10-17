// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * This instance's servers.dat -- the widget-free replacement for
 * ServersPage. Laid out like the game's own server list: one read-only row
 * per server (the favicon the game last saw, name, address), "Join" to
 * launch the instance straight into
 * it, and the rest -- edit, reorder, remove -- behind the row's ⋯ menu (also
 * on right-click). Adding or editing a server opens one small dialog.
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

    readonly property int rowHeight: 64
    readonly property int thumbSize: 40
    // The same frame InstanceListRow gives an instance icon.
    readonly property int thumbInset: Theme.space.xs
    // servers.dat's per-server resource pack choice, in its own order.
    readonly property var packChoices: [
        { value: 0, label: qsTr("Ask") },
        { value: 1, label: qsTr("Always") },
        { value: 2, label: qsTr("Never") }
    ]

    // A caption over one of the server dialog's fields.
    component FieldLabel: Text {
        width: parent ? parent.width : implicitWidth
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }

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
            // Secondary: the play bar's Play is this page's primary action.
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
            // Overlays the rows' right edge rather than reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string address
                required property int acceptTextures
                // The favicon the game saved when it last pinged the
                // server, as a data: URL; "" if it never has.
                required property string iconUrl

                readonly property string title: row.name.length > 0 ? row.name : qsTr("Minecraft Server")

                width: list.width
                height: root.rowHeight
                radius: Theme.radius.lg
                color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border
                Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                HoverHandler { id: hover }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: rowMenu.popup()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.sm
                    spacing: Theme.space.md

                    Rectangle {
                        Layout.preferredWidth: root.thumbSize
                        Layout.preferredHeight: root.thumbSize
                        radius: Theme.radius.sm
                        color: Theme.palette.surfaceSunken
                        Image {
                            id: favicon
                            anchors.fill: parent
                            anchors.margins: root.thumbInset
                            source: row.iconUrl
                            sourceSize: Qt.size(width, height)
                            fillMode: Image.PreserveAspectFit
                        }
                        MeshIcon {
                            anchors.centerIn: parent
                            visible: favicon.status !== Image.Ready
                            iconName: "server"
                            size: Theme.icon.sm
                            color: Theme.palette.textTertiary
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Theme.space.xxs
                        Text {
                            width: parent.width
                            text: row.title
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.bodyStrong.pixelSize
                            font.weight: Theme.type.bodyStrong.weight
                        }
                        Text {
                            width: parent.width
                            text: row.address.length > 0 ? row.address : qsTr("No address yet")
                            elide: Text.ElideRight
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
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

    // Rows fade out at the fold instead of being sliced by the play bar.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: list
    }

    // Add (row -1) or edit one server: the three things servers.dat keeps.
    Dialog {
        id: serverDialog
        property int row: -1
        property string name
        property string address
        property int acceptTextures: 0
        // The resource pack choice being edited (one of root.packChoices).
        property int packChoice: 0

        title: row < 0 ? qsTr("Add server") : qsTr("Edit server")
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, parent ? parent.width - Theme.space.xxl * 2 : 440)
        modal: true

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
                        // The dialog owns the choice, so the group is not
                        // exclusive by itself; a click sets it and puts the
                        // binding back.
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

        footer: Row {
            layoutDirection: Qt.RightToLeft
            spacing: Theme.space.sm
            padding: Theme.space.lg
            topPadding: 0
            Button {
                text: serverDialog.row < 0 ? qsTr("Add server") : qsTr("Save")
                highlighted: true
                enabled: addressField.text.trim().length > 0
                onClicked: serverDialog.save()
            }
            Button {
                text: qsTr("Cancel")
                flat: true
                onClicked: serverDialog.close()
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
        MeshIcon { iconName: "server"; size: 40; color: Theme.palette.textTertiary }
    }
}
