// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQml.Models
import Kite.Theme

/*
 * Pick an instance icon from every icon Kite knows (the built-in set and the
 * icons folder). A plain grid of pictures; the names are file keys, not words,
 * so they are only read out to a screen reader.
 */
AppDialog {
    id: root

    // IconList (roles include `key` and `name`).
    property var iconsModel: null
    property string current
    signal picked(string key)
    signal openFolderRequested()

    size: "lg"
    height: Math.min(520, parent ? parent.height - Theme.space.xxl * 2 : 520)
    closeOnOutsidePress: true
    closeOnPrimary: false
    title: qsTr("Choose an icon")
    primaryText: qsTr("Use icon")
    onPrimaryClicked: { root.picked(root.current); root.close() }
    footerLeading: [
        Button {
            flat: true
            text: qsTr("Open icons folder")
            icon.source: Icons.url("folder")
            onClicked: root.openFolderRequested()
        }
    ]

    // The *_legacy and *_mark icons would list every icon twice, so they stay out of the picker; an
    // instance already using one keeps it.
    DelegateModel {
        id: shownIcons
        model: root.iconsModel
        groups: [ DelegateModelGroup { id: shownGroup; name: "shown" } ]
        filterOnGroup: "shown"
        delegate: iconCell

        function refilter() {
            for (var i = 0; i < items.count; ++i) {
                var item = items.get(i)
                var key = String(item.model.key)
                item.inShown = (!key.endsWith("_legacy") && !key.endsWith("_mark")) || key === root.current
            }
        }
        items.onChanged: refilter()
        Component.onCompleted: refilter()
    }

    readonly property int iconSize: 48
    readonly property int cellSize: 72
    readonly property int cellInset: Theme.space.xs

    contentItem: GridView {
        id: grid
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: shownIcons
        // As many square cells as fit, stretched to fill the row.
        readonly property int columns: Math.max(1, Math.floor(width / root.cellSize))
        cellWidth: Math.floor(width / columns)
        cellHeight: root.cellSize
        ScrollBar.vertical: ScrollBar {}
    }

    Component {
        id: iconCell

        AbstractButton {
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

            background: StateFill {
                anchors.fill: parent
                anchors.margins: root.cellInset
                radius: Theme.radius.sm
                hovered: cell.hovered
                active: cell.selected
                activeFill: Theme.palette.surfaceHover
                border.width: cell.selected ? 2 : 0
                border.color: Theme.palette.accent
            }

            contentItem: Item {
                Image {
                    anchors.centerIn: parent
                    width: root.iconSize
                    height: root.iconSize
                    source: Icons.instanceIcon(cell.key)
                    sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
            }
        }
    }
}
