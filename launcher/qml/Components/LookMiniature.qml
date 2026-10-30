// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * One look drawn as a miniature launcher in its own colours, for LookPicker:
 * Sahne's stage across the top with a white Play and a row of cards, Sade's
 * hairline rows under a green Play, Gece's night sky with glass rows, and
 * Klasik's cards (PalettePicker's drawing) in the palette picked for it.
 * The shapes are blocks, not words: the choice is made by looking.
 */
Rectangle {
    id: root

    // "stage", "calm", "night" or "classic".
    required property string look
    // That look's palette (Theme.previewPalette()).
    required property var colors

    readonly property int sidebarWidth: 22
    readonly property int gap: 6
    readonly property int inset: 8
    readonly property real innerX: root.sidebarWidth + root.inset
    readonly property real innerWidth: root.width - root.innerX - root.inset
    readonly property int playWidth: 30
    readonly property int playHeight: 9
    readonly property int titleWidth: 46
    readonly property int barHeight: 4
    readonly property int stageHeight: 44
    readonly property int rowHeight: 12
    // Gece's sky, as SkyBackdrop draws it.
    readonly property var skyStars: [[0.42, 0.12], [0.63, 0.2], [0.81, 0.08], [0.9, 0.3], [0.55, 0.38], [0.74, 0.46]]

    radius: Theme.radius.md
    color: root.colors.canvas
    border.width: 1
    border.color: root.colors.border
    clip: true

    // -- Gece: the sky behind everything ---------------------------------
    SkyBackdrop {
        anchors.fill: parent
        anchors.margins: 1
        visible: root.look === "night"
        starCount: 0
    }
    Repeater {
        model: root.look === "night" ? root.skyStars : []
        delegate: Rectangle {
            required property var modelData
            x: modelData[0] * root.width
            y: modelData[1] * root.height
            width: 2; height: 2
            color: "white"
            opacity: 0.6
        }
    }

    Rectangle {
        width: root.sidebarWidth
        height: parent.height
        color: root.look === "night" ? "transparent" : root.colors.surface
    }

    // -- Sahne: the stage, its title, Play, then four cards --------------
    Item {
        anchors.fill: parent
        visible: root.look === "stage"

        Rectangle {
            x: root.sidebarWidth
            width: parent.width - root.sidebarWidth
            height: root.stageHeight
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: root.colors.surfaceSunken }
                GradientStop { position: 1.0; color: root.colors.surfaceRaised }
            }
        }
        Rectangle { x: root.innerX; y: 12; width: root.titleWidth; height: root.barHeight + 1; radius: 1; color: root.colors.textPrimary }
        Rectangle { x: root.innerX; y: 26; width: root.playWidth; height: root.playHeight; radius: 2; color: root.colors.accent }
        Row {
            x: root.innerX
            y: root.stageHeight + root.gap
            spacing: root.gap / 2
            Repeater {
                model: 4
                delegate: Rectangle {
                    width: (root.innerWidth - root.gap * 1.5) / 4
                    height: 26
                    radius: 3
                    color: root.colors.surfaceRaised
                }
            }
        }
    }

    // -- Sade: a flat hero line, green Play, hairline rows ---------------
    Item {
        anchors.fill: parent
        visible: root.look === "calm"

        Rectangle { x: root.innerX; y: 12; width: 12; height: 12; radius: 3; color: root.colors.surfaceRaised }
        Rectangle { x: root.innerX + 18; y: 15; width: root.titleWidth - 8; height: root.barHeight + 1; radius: 1; color: root.colors.textPrimary }
        Rectangle { x: root.width - root.inset - root.playWidth; y: 13; width: root.playWidth; height: root.playHeight; radius: 2; color: root.colors.accent }
        Repeater {
            model: 3
            delegate: Item {
                required property int index
                x: root.innerX
                y: 36 + index * (root.rowHeight + 2)
                width: root.innerWidth
                height: root.rowHeight
                Rectangle { y: 2; width: 8; height: 8; radius: 2; color: root.colors.surfaceRaised }
                Rectangle { x: 13; y: 4; width: parent.width * 0.45; height: root.barHeight; radius: 1; color: root.colors.textTertiary }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: root.colors.divider }
            }
        }
    }

    // -- Gece: title and Play on the sky, glass rows two by two ----------
    Item {
        anchors.fill: parent
        visible: root.look === "night"

        Rectangle { x: root.innerX; y: 12; width: root.titleWidth; height: root.barHeight + 1; radius: 1; color: root.colors.textPrimary }
        Rectangle { x: root.innerX; y: 24; width: root.playWidth; height: root.playHeight; radius: 2; color: root.colors.accent }
        Grid {
            x: root.innerX
            y: 44
            columns: 2
            spacing: root.gap / 2
            Repeater {
                model: 4
                delegate: Rectangle {
                    width: (root.innerWidth - root.gap / 2) / 2
                    height: 15
                    radius: 3
                    color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.1)
                }
            }
        }
    }

    // -- Klasik: PalettePicker's three cards and Play --------------------
    Item {
        anchors.fill: parent
        visible: root.look === "classic"

        Row {
            x: root.innerX
            y: 10
            spacing: root.gap
            Repeater {
                model: 3
                delegate: Rectangle {
                    width: (root.innerWidth - root.gap * 2) / 3
                    height: 30
                    radius: Theme.radius.sm
                    color: root.colors.surfaceRaised
                    border.width: 1
                    border.color: root.colors.border
                }
            }
        }
        Rectangle {
            x: root.innerX + (root.innerWidth - width) / 2
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            width: 64; height: 18
            radius: Theme.radius.sm
            color: root.colors.accent
        }
    }
}
