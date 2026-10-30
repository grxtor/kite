// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The launcher's looks side by side (Settings > Appearance), each a
 * miniature launcher in its own colours and layout (LookMiniature): Sahne,
 * Sade, Gece, and Klasik, the look whose palette and light or dark mode are
 * picked below it. Laid out like PalettePicker: four in a row while they
 * fit at a readable size, otherwise two rows of two.
 */
Grid {
    id: root

    // The look shown as chosen (Theme.look).
    property string current: Theme.look
    signal picked(string look)

    readonly property var looks: [
        { id: "stage", label: qsTr("Stage"), note: qsTr("A big stage, a white Play") },
        { id: "calm", label: qsTr("Calm"), note: qsTr("Plain rows, a green Play") },
        { id: "night", label: qsTr("Night"), note: qsTr("Kite's night sky") },
        { id: "classic", label: qsTr("Classic"), note: qsTr("Your palette, light or dark") }
    ]

    // The narrowest tile whose longest note still fits on one line; below
    // it the tiles go two by two.
    readonly property int minTileWidth: 170
    readonly property int tileHeight: 150
    readonly property bool fitsInOneRow: (width - spacing * (looks.length - 1)) / looks.length >= minTileWidth
    readonly property real tileWidth: (width - spacing * (columns - 1)) / columns

    width: parent ? parent.width : implicitWidth
    columns: root.fitsInOneRow ? root.looks.length : 2
    spacing: Theme.space.md

    Repeater {
        model: root.looks

        delegate: PickerTile {
            id: tile
            required property var modelData
            width: root.tileWidth
            height: root.tileHeight
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
