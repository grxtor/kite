// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The colour schemes side by side, each drawn as a miniature launcher in its own
 * colours for the current light or dark mode. A PickerGrid of equal tiles, never a
 * wrapping Flow.
 */
PickerGrid {
    id: root

    property string current: Theme.scheme
    signal picked(string scheme)

    readonly property var schemes: [
        { id: "grass", label: qsTr("Grass"), note: qsTr("Graphite and grass green") },
        { id: "amethyst", label: qsTr("Obsidian"), note: qsTr("Graphite and amethyst") },
        { id: "ember", label: qsTr("Ember"), note: qsTr("Charcoal and lava") },
        { id: "diamond", label: qsTr("Diamond"), note: qsTr("Navy and diamond blue") }
    ]

    readonly property int tileHeight: 150
    readonly property int previewHeight: 86
    readonly property int miniSidebarWidth: 34
    readonly property int miniGap: 6

    tileCount: root.schemes.length
    // The narrowest tile whose longest note still fits on one line.
    minTileWidth: 160

    Repeater {
        model: root.schemes

        delegate: PickerTile {
            id: card
            required property var modelData
            // Re-read when the mode flips, so the previews follow it.
            readonly property var colors: Theme.dark, Theme.previewPalette(modelData.id)

            width: root.tileWidth
            height: root.tileHeight
            previewHeight: root.previewHeight
            label: modelData.label
            note: modelData.note
            chosen: root.current === modelData.id
            onClicked: root.picked(modelData.id)

            Rectangle {
                id: preview
                anchors.fill: parent
                radius: Theme.radius.md
                color: card.colors.canvas
                clip: true

                Rectangle {
                    id: miniSidebar
                    x: 0
                    width: root.miniSidebarWidth
                    height: parent.height
                    color: card.colors.surface
                    Rectangle {
                        x: 6; y: 10
                        width: 22; height: 6
                        radius: Theme.radius.xs
                        color: card.colors.accentSubtle
                        Rectangle { width: 2; height: parent.height; color: card.colors.accent }
                    }
                    Repeater {
                        model: 2
                        delegate: Rectangle {
                            required property int index
                            x: 6; y: 22 + index * 10
                            width: 18; height: 4
                            radius: Theme.radius.xs
                            color: card.colors.textTertiary
                            opacity: 0.6
                        }
                    }
                }
                Row {
                    id: miniCards
                    x: miniSidebar.width + root.miniGap
                    y: 10
                    spacing: root.miniGap
                    Repeater {
                        model: 3
                        delegate: Rectangle {
                            width: (preview.width - miniCards.x - root.miniGap * 3) / 3
                            height: 30
                            radius: Theme.radius.sm
                            color: card.colors.surfaceRaised
                            border.width: 1
                            border.color: card.colors.border
                        }
                    }
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.horizontalCenterOffset: miniSidebar.width / 2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    width: 64; height: 18
                    radius: Theme.radius.sm
                    color: card.colors.accent
                }
            }
        }
    }
}
