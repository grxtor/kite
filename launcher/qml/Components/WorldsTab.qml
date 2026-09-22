// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's single-player worlds, most useful facts first: name,
 * game mode, when it was last played. Deleting one asks first -- a world
 * is hours of someone's play and there is no undo.
 */
Item {
    id: root

    // InstanceDetails: worlds (WorldList: folder, seed, name, gameMode,
    // lastPlayed, iconFile, dayCount), worldsDir, deleteWorld.
    property var details: null
    readonly property bool unlocked: !!details && details.contentChangesAllowed
    readonly property int count: list.count
    signal openFolderRequested(string path)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: list.count > 0 ? qsTr("%1 worlds").arg(list.count) : ""
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.details ? root.details.worldsDir : "")
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.details ? root.details.worlds : null
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string folder
                required property var gameMode
                required property var lastPlayed
                required property var iconFile

                width: list.width - Theme.space.md
                height: 72
                radius: Theme.radius.lg
                color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.md
                    spacing: Theme.space.md

                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: Theme.radius.md
                        color: Theme.palette.surfaceSunken
                        Image {
                            id: worldIcon
                            anchors.fill: parent
                            source: Format.fileUrl(row.iconFile)
                            sourceSize: Qt.size(96, 96)
                            smooth: false
                            visible: status === Image.Ready
                        }
                        MeshIcon {
                            anchors.centerIn: parent
                            visible: !worldIcon.visible
                            iconName: "globe"
                            color: Theme.palette.textTertiary
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            width: parent.width
                            text: row.name.length > 0 ? row.name : row.folder
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: [String(row.gameMode || ""),
                                   Format.lastPlayed(row.lastPlayed ? Number(row.lastPlayed) : 0)]
                                  .filter(t => t.length > 0).join("  \u00b7  ")
                            elide: Text.ElideRight
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }

                    IconButton {
                        visible: hover.hovered && root.unlocked
                        iconName: "trash"
                        tip: qsTr("Delete world")
                        onClicked: {
                            confirm.row = row.index
                            confirm.text = qsTr("Delete the world \u201c%1\u201d? It cannot be recovered from the launcher.").arg(row.name.length > 0 ? row.name : row.folder)
                            confirm.open()
                        }
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirm
        property int row: -1
        title: qsTr("Delete world")
        confirmText: qsTr("Delete world")
        onConfirmed: if (root.details) root.details.deleteWorld(row)
    }

    EmptyState {
        anchors.centerIn: parent
        visible: list.count === 0
        title: qsTr("No worlds yet")
        body: qsTr("Worlds you create in single player show up here.")
        MeshIcon { iconName: "globe"; size: 40; color: Theme.palette.textTertiary }
    }
}
