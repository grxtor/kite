// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// The brief's glass treatment as one piece: the surface, the hairline
// border, a 1 px light line along the top edge with a faint sheen fading
// down from it and, when given a `level`, Theme's soft Shadow underneath. Only the sidebar, the bottom
// play bar and popups/dialogs use it (see Theme.glass); everything else is
// an ordinary opaque surface.
//
// Popups take the default fill (Theme.glass.popup); the sidebar and play
// bar set `color: Theme.glass.fill` and usually no level.
Rectangle {
    id: glass

    // Theme.elevation.sm/md/lg, or null for no shadow.
    property QtObject level: null

    color: Theme.glass.popup
    border.width: 1
    border.color: Theme.glass.border

    Shadow {
        visible: glass.level !== null
        radius: glass.radius
        level: glass.level || Theme.elevation.md
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, glass.radius - 1)
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.glass.sheen }
            GradientStop { position: 0.5; color: "transparent" }
        }
    }

    // Inset by the corner so the line never pokes past the rounding.
    Rectangle {
        x: Math.max(glass.radius, 1)
        y: 1
        width: glass.width - x * 2
        height: 1
        color: Theme.glass.highlight
    }
}
