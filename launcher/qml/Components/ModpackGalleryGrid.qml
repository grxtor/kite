// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Every gallery picture of a modpack (ModpackDetail's "Gallery" tab) in a grid that fills the column, each with its
 * title under it when the provider gives one. A click asks for it large (opened(index)).
 */
Flow {
    id: root

    // [{ url, title? }]
    property var images: []
    signal opened(int index)

    readonly property int minCellWidth: 240
    readonly property int columns: Math.max(1, Math.floor((width + spacing) / (minCellWidth + spacing)))
    readonly property int cellWidth: Math.floor((width - spacing * (columns - 1)) / columns)

    spacing: Theme.space.md

    Repeater {
        model: root.images
        delegate: Column {
            id: cell
            required property var modelData
            required property int index
            width: root.cellWidth
            spacing: Theme.space.xs

            AbstractButton {
                id: shot
                width: parent.width
                height: Math.round(width * 9 / 16)
                hoverEnabled: true
                Accessible.name: (cell.modelData.title || "").length > 0
                                 ? cell.modelData.title : qsTr("Picture %1").arg(cell.index + 1)
                onClicked: root.opened(cell.index)

                background: Rectangle {
                    radius: Theme.radius.md
                    color: Theme.palette.surfaceSunken
                }
                contentItem: Item {
                    Image {
                        anchors.fill: parent
                        source: cell.modelData.url || ""
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        asynchronous: true
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: shot.hovered || shot.visualFocus
                        color: Theme.palette.hoverOverlay
                    }
                    CornerMatte {
                        cornerRadius: Theme.radius.md
                        matte: Theme.palette.canvas
                        outline: shot.visualFocus ? Theme.palette.textPrimary : Theme.palette.border
                    }
                }
            }
            Copy {
                width: parent.width
                visible: text.length > 0
                text: cell.modelData.title || ""
                elide: Text.ElideRight
                role: "label"
                tone: "secondary"
            }
        }
    }
}
