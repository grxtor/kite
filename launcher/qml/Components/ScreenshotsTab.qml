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

    // The narrowest a thumbnail gets before the grid drops a column.
    readonly property int minTileWidth: 240
    readonly property int gap: Theme.space.md
    // Under each thumbnail: its name and date, two caption lines.
    readonly property int captionSpacing: 1
    readonly property int captionHeight: Theme.space.xs + Math.ceil(captionMetrics.height) * 2 + root.captionSpacing

    FontMetrics {
        id: captionMetrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
    }

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
            // Each cell carries one gap on its right; the grid is a gap
            // wider than the page (so `width` below includes it) and the
            // last column's gap falls outside the content edge, so the
            // thumbnails run flush with the toolbar's edges.
            Layout.rightMargin: -root.gap
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            readonly property int columns: Math.max(1, Math.floor(width / (root.minTileWidth + root.gap)))
            readonly property int tileWidth: Math.floor(width / columns) - root.gap
            readonly property int tileHeight: Math.round(tileWidth * 9 / 16)
            cellWidth: tileWidth + root.gap
            // The caption, then the same gap as between columns.
            cellHeight: tileHeight + root.captionHeight + root.gap
            // Overlays the thumbnails rather than reserving a gutter.
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
                    width: grid.tileWidth
                    height: grid.tileHeight
                    radius: Theme.radius.lg
                    color: Theme.palette.surfaceSunken
                    clip: true

                    Image {
                        id: thumb
                        anchors.fill: parent
                        source: "image://screenshot/" + encodeURIComponent(cell.path)
                        sourceSize: Qt.size(512, 512)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
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

                    CornerMatte {
                        cornerRadius: frame.radius
                        outline: hover.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    }
                }

                Column {
                    anchors.top: frame.bottom
                    anchors.topMargin: Theme.space.xs
                    x: frame.x
                    width: frame.width
                    spacing: root.captionSpacing

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

    // Rows fade out at the fold instead of being sliced by the play bar.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: grid
    }

    EmptyState {
        anchors.centerIn: parent
        visible: !root.model || root.model.count === 0
        title: qsTr("No screenshots yet")
        body: qsTr("Press F2 in game; they will show up here.")
        MeshIcon { iconName: "image"; size: 40; color: Theme.palette.textTertiary }
    }
}
