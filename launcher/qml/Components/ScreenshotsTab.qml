// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's screenshots as a grid of thumbnails, newest first.
 * Hovering one raises a scrim with its actions; clicking the image itself
 * opens it in the system viewer. Thumbnails are decoded off the UI thread
 * by the "screenshot" image provider.
 */
Item {
    id: root

    // ScreenshotListModel: name, path, url, modified, size.
    property var model: null
    property string directory
    signal openFolderRequested(string path)

    readonly property int tileWidth: 240
    readonly property int tileHeight: Math.round(tileWidth * 9 / 16)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: root.model && root.model.count > 0
                      ? qsTr("%1 screenshots").arg(root.model.count) : ""
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.directory)
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            readonly property int columns: Math.max(1, Math.floor(width / (root.tileWidth + Theme.space.md)))
            cellWidth: Math.floor(width / columns)
            cellHeight: root.tileHeight + Theme.space.xl + Theme.space.lg
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property int index
                required property string name
                required property string path
                required property string url
                required property var modified

                width: grid.cellWidth
                height: grid.cellHeight

                HoverHandler { id: hover }

                Rectangle {
                    id: frame
                    x: (parent.width - width) / 2
                    width: grid.cellWidth - Theme.space.md
                    height: root.tileHeight
                    radius: Theme.radius.md
                    color: Theme.palette.surfaceSunken
                    border.width: 1
                    border.color: hover.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    clip: true

                    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }

                    Image {
                        id: thumb
                        anchors.fill: parent
                        anchors.margins: 1
                        source: "image://screenshot/" + encodeURIComponent(cell.path)
                        sourceSize: Qt.size(512, 512)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        scale: hover.hovered ? 1.04 : 1.0
                        Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                    }

                    // A scrim over the foot of the thumbnail, dark regardless
                    // of theme (like Tag's onMedia mode), so the actions on
                    // top of it stay readable over any screenshot.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.42
                        opacity: hover.hovered ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
                        }

                        RowLayout {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Theme.space.xs
                            spacing: Theme.space.xxs

                            IconButton {
                                size: Theme.control.heightSm
                                flat: false
                                iconName: "external-link"
                                tip: qsTr("Open")
                                onClicked: Qt.openUrlExternally(cell.url)
                            }
                            IconButton {
                                size: Theme.control.heightSm
                                flat: false
                                iconName: "trash"
                                tip: qsTr("Move to trash")
                                onClicked: if (root.model) root.model.remove(cell.index)
                            }
                        }
                    }

                    TapHandler { onTapped: Qt.openUrlExternally(cell.url) }
                }

                Column {
                    anchors.top: frame.bottom
                    anchors.topMargin: Theme.space.xs
                    x: frame.x
                    width: frame.width
                    spacing: 1

                    Text {
                        width: parent.width
                        text: cell.name
                        elide: Text.ElideMiddle
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.Medium
                    }
                    Text {
                        width: parent.width
                        text: Format.lastPlayed(cell.modified ? Number(cell.modified) : 0)
                        elide: Text.ElideRight
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: !root.model || root.model.count === 0
        title: qsTr("No screenshots yet")
        body: qsTr("Press F2 in game; they will show up here.")
        MeshIcon { iconName: "image"; size: 40; color: Theme.palette.textTertiary }
    }
}
