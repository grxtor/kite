// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * This instance's servers.dat -- the widget-free replacement for
 * ServersPage. Every field edits in place (no separate "selected server"
 * side panel the way the widget had one, since a row's own fields are right
 * there); "Join" launches the instance straight into that address.
 */
Item {
    id: root

    // InstanceDetails.servers (ServersListModel) + serversDir.
    property var model: null
    property string serversDir: ""
    readonly property bool unlocked: !!root.model && !root.model.locked
    readonly property int count: list.count
    signal openFolderRequested(string path)
    signal joinRequested(string address)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Text {
                Layout.fillWidth: true
                text: !root.unlocked && !!root.model
                      ? qsTr("The game is running; the server list can be changed once it has closed.")
                      : list.count > 0 ? qsTr("%1 servers").arg(list.count) : ""
                color: root.unlocked ? Theme.palette.textTertiary : Theme.palette.warning
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.serversDir)
            }
            Button {
                enabled: root.unlocked
                text: qsTr("Add server")
                icon.source: Icons.url("plus")
                onClicked: {
                    if (!root.model)
                        return
                    var row = root.model.addServer()
                    if (row >= 0)
                        list.positionViewAtIndex(row, ListView.Contain)
                }
            }
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

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string address
                required property int acceptTextures

                width: list.width - Theme.space.md
                height: Theme.control.heightLg * 2 + Theme.space.lg
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space.md
                    spacing: Theme.space.md

                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        Layout.alignment: Qt.AlignTop
                        radius: Theme.radius.md
                        color: Theme.palette.surfaceSunken
                        MeshIcon {
                            anchors.centerIn: parent
                            iconName: "server"
                            size: Theme.icon.sm
                            color: Theme.palette.textTertiary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.space.xs

                        TextField {
                            Layout.fillWidth: true
                            enabled: root.unlocked
                            text: row.name
                            placeholderText: qsTr("Name")
                            Accessible.name: qsTr("Server name")
                            onEditingFinished: if (root.model) root.model.setName(row.index, text)
                        }
                        TextField {
                            Layout.fillWidth: true
                            enabled: root.unlocked
                            text: row.address
                            placeholderText: qsTr("address:port")
                            Accessible.name: qsTr("Server address")
                            onEditingFinished: if (root.model) root.model.setAddress(row.index, text)
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space.xs

                        Text {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Resource packs")
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                        SegmentedControl {
                            Layout.alignment: Qt.AlignRight
                            enabled: root.unlocked
                            options: [
                                { value: 0, label: qsTr("Ask") },
                                { value: 1, label: qsTr("Always") },
                                { value: 2, label: qsTr("Never") }
                            ]
                            current: row.acceptTextures
                            onActivated: (value) => { if (root.model) root.model.setAcceptTextures(row.index, value) }
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space.xs

                        RowLayout {
                            spacing: Theme.space.xs
                            IconButton {
                                iconName: "arrow-up"
                                tip: qsTr("Move up")
                                enabled: root.unlocked && row.index > 0
                                onClicked: if (root.model) root.model.moveUp(row.index)
                            }
                            IconButton {
                                iconName: "arrow-down"
                                tip: qsTr("Move down")
                                enabled: root.unlocked && row.index < list.count - 1
                                onClicked: if (root.model) root.model.moveDown(row.index)
                            }
                            IconButton {
                                iconName: "trash"
                                tip: qsTr("Remove server")
                                enabled: root.unlocked
                                onClicked: {
                                    confirm.row = row.index
                                    confirm.text = qsTr("Remove “%1” from this instance's server list?").arg(row.name.length > 0 ? row.name : row.address)
                                    confirm.open()
                                }
                            }
                        }
                        Button {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Join")
                            icon.source: Icons.url("play")
                            enabled: row.address.trim().length > 0
                            onClicked: root.joinRequested(row.address)
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
        body: qsTr("Servers this instance has joined show up here, or add one by hand.")
        actionText: qsTr("Add server")
        onActionTriggered: if (root.model) root.model.addServer()
        MeshIcon { iconName: "server"; size: 40; color: Theme.palette.textTertiary }
    }
}
