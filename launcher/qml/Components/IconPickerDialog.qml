// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQml.Models
import Kite.Theme

/*
 * Pick an instance icon from every icon Kite knows -- the built-in set
 * and whatever is in the icons folder. New icons are added by dropping
 * images into that folder; the grid picks them up as they arrive.
 *
 * A plain grid of pictures, as in Prism: the icons' names are file keys
 * ("ftb_logo", "enderpearl"), not words, so they are not shown -- only read
 * out to a screen reader.
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
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    title: qsTr("Choose an icon")

    header: DialogHeader {
        title: root.title
        icon: "image"
    }

    // The built-in *_legacy icons are the old pixel-art set, kept on disk
    // for instances that still name them; offered here next to the flat set
    // they would only list every icon twice. The *_mark icons are
    // the loader marks the New instance dialog's loader row uses, duplicates
    // of the forge/quilt icons already listed. Both stay out of the picker;
    // an instance already using one keeps it.
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

    // Each icon's picture, and the square cell around it.
    readonly property int iconSize: 48
    readonly property int cellSize: 72
    readonly property int cellInset: Theme.space.xs

    contentItem: GridView {
        id: grid
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: shownIcons
        // As many square cells as fit, then stretched to fill the row, so
        // the grid has no empty strip on its right.
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

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: root.cellInset
                radius: Theme.radius.sm
                color: cell.selected ? Theme.palette.surfaceHover
                     : cell.hovered ? Theme.palette.hoverOverlay : "transparent"
                border.width: cell.selected ? 2 : 0
                border.color: Theme.palette.accent
            }

            contentItem: Item {
                Image {
                    anchors.centerIn: parent
                    width: root.iconSize
                    height: root.iconSize
                    source: "image://instanceicon/" + cell.key
                    sourceSize: Qt.size(root.iconSize * 2, root.iconSize * 2)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
            }
        }
    }

    footer: Row {
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: 0
        layoutDirection: Qt.RightToLeft

        Button {
            text: qsTr("Use icon")
            highlighted: true
            onClicked: { root.picked(root.current); root.close() }
        }
        Button {
            text: qsTr("Cancel")
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
