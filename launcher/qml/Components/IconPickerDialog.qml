// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Pick an instance icon from every icon MeshMC knows -- the built-in set
 * and whatever is in the icons folder. New icons are added by dropping
 * images into that folder; the grid picks them up as they arrive.
 */
Dialog {
    id: root

    // IconList (roles include `key` and `name`).
    property var iconsModel: null
    property string current
    signal picked(string key)
    signal openFolderRequested()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(640, parent ? parent.width - Theme.space.xxl * 2 : 640)
    height: Math.min(520, parent ? parent.height - Theme.space.xxl * 2 : 520)
    modal: true
    title: qsTr("Choose an icon")

    contentItem: GridView {
        id: grid
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.iconsModel
        cellWidth: 88
        cellHeight: 96
        ScrollBar.vertical: ScrollBar {}

        delegate: AbstractButton {
            id: cell
            required property string key
            required property string name
            readonly property bool selected: key === root.current

            width: grid.cellWidth
            height: grid.cellHeight
            hoverEnabled: true
            Accessible.name: name
            onClicked: root.current = key
            onDoubleClicked: { root.picked(key); root.close() }

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: Theme.radius.md
                color: cell.selected ? Theme.palette.accentSubtle
                     : cell.hovered ? Theme.palette.hoverOverlay : "transparent"
                border.width: cell.selected ? 2 : 0
                border.color: Theme.palette.accent
            }

            contentItem: Column {
                spacing: Theme.space.xs
                topPadding: Theme.space.sm
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 48
                    height: 48
                    source: "image://instanceicon/" + cell.key
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    width: cell.width - Theme.space.sm * 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.name
                    elide: Text.ElideRight
                    color: cell.selected ? Theme.palette.textPrimary : Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
        }
    }

    footer: Row {
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: Theme.space.sm
        layoutDirection: Qt.RightToLeft

        Button {
            text: qsTr("Use icon")
            highlighted: true
            onClicked: { root.picked(root.current); root.close() }
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
        Button {
            flat: true
            text: qsTr("Open icons folder")
            icon.source: Icons.url("folder")
            onClicked: root.openFolderRequested()
        }
    }
}
