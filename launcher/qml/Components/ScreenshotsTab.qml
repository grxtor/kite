// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's screenshots as a grid of thumbnails, newest first.
 * Clicking one opens it in the system viewer; thumbnails are decoded off
 * the UI thread by the "screenshot" image provider.
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
            cellHeight: root.tileHeight + Theme.space.xl + Theme.space.md
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property int index
                required property string name
                required property string path
                required property string url

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

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: "image://screenshot/" + encodeURIComponent(cell.path)
                        sourceSize: Qt.size(512, 512)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }

                    TapHandler { onTapped: Qt.openUrlExternally(cell.url) }

                    IconButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.space.xs
                        size: Theme.control.heightSm
                        flat: false
                        visible: hover.hovered
                        iconName: "trash"
                        tip: qsTr("Move to trash")
                        onClicked: if (root.model) root.model.remove(cell.index)
                    }
                }

                Text {
                    anchors.top: frame.bottom
                    anchors.topMargin: Theme.space.xs
                    x: frame.x
                    width: frame.width
                    text: cell.name
                    elide: Text.ElideMiddle
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
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
