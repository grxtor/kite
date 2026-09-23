// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A shimmering placeholder block for content still loading -- a flat tinted
 * rectangle with a soft highlight band sweeping across it on a loop. No
 * shader (the Qt floor here is 6.4): the "shine" is an ordinary horizontal
 * Gradient translated by a NumberAnimation, clipped to this item's bounds.
 *
 * Generic on purpose, so any area can drop it in wherever a spinner would
 * otherwise sit -- a card grid (see ModpackCard's `skeleton` mode), a list
 * row, a single line of text.
 */
Item {
    id: root

    property int radius: Theme.radius.md

    clip: true

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.palette.surfaceSunken
    }

    Rectangle {
        id: sweep
        width: Math.max(1, root.width) * 0.4
        height: parent.height
        radius: root.radius
        // Transparent -> a token's own colour -> transparent, so the sweep
        // reads as a highlight in both themes without a literal colour.
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0) }
            GradientStop { position: 0.5; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0.85) }
            GradientStop { position: 1.0; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0) }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            running: root.visible
            NumberAnimation { from: -sweep.width; to: root.width; duration: 1300; easing.type: Easing.InOutSine }
            PauseAnimation { duration: 450 }
        }
    }
}
