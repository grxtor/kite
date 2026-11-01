// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The launcher's looks side by side (Settings > Appearance), each a LookMiniature,
 * laid out by PickerGrid.
 */
PickerGrid {
    id: root

    property string current: Theme.look
    signal picked(string look)

    readonly property var looks: [
        { id: "stage", label: qsTr("Stage"), note: qsTr("A big stage, a white Play") },
        { id: "calm", label: qsTr("Calm"), note: qsTr("Plain rows, a green Play") },
        { id: "night", label: qsTr("Night"), note: qsTr("Kite's night sky") },
        { id: "classic", label: qsTr("Classic"), note: qsTr("Your palette, light or dark") }
    ]

    readonly property int tilePadding: Theme.space.sm
    readonly property int captionHeight: 64
    readonly property int previewHeight: Math.round((root.tileWidth - root.tilePadding * 2) * root.miniatureAspect)
    readonly property real miniatureAspect: 180 / 320
    readonly property int tileHeight: root.previewHeight + root.captionHeight

    tileCount: root.looks.length
    // The narrowest tile whose miniature still reads.
    minTileWidth: 220

    Repeater {
        model: root.looks

        delegate: PickerTile {
            id: tile
            required property var modelData
            width: root.tileWidth
            height: root.tileHeight
            previewHeight: root.previewHeight
            label: modelData.label
            note: modelData.note
            chosen: root.current === modelData.id
            previewOutline: miniature.colors.border
            onClicked: root.picked(modelData.id)

            LookMiniature {
                id: miniature
                anchors.fill: parent
                look: tile.modelData.id
                // Klasik shows the palette picked for it; a look its own.
                // Re-read when the mode flips.
                colors: Theme.dark, Theme.previewPalette(tile.modelData.id === "classic"
                                                         ? Theme.scheme : tile.modelData.id)
            }
        }
    }
}
