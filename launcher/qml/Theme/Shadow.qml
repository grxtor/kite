// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// A drop shadow under a raised surface, drawn as a stack of translucent
// rounded rects fanning out from its edge, since Qt 6.4 has no MultiEffect to
// blur a real one; the fade towards the rim reads as a blur.
//
// Place it as a child of the surface Rectangle, with a matching `radius` and
// `level` one of Theme.elevation's sm/md/lg. It sits under its parent (z -1),
// so it never darkens the surface. Fade it with `opacity`, never by changing
// `level`.
Item {
    id: shadow

    property real radius: 0
    property QtObject level: Theme.elevation.md
    property color color: Theme.elevation.color

    readonly property int layers: 8

    z: -1
    anchors.fill: parent
    visible: opacity > 0

    Repeater {
        model: shadow.layers

        Rectangle {
            required property int index
            // 1/layers for the innermost rect up to 1 for the outermost.
            readonly property real reach: (index + 1) / shadow.layers
            readonly property real spread: shadow.level.blur * 0.6 * reach

            x: -spread
            y: -spread + shadow.level.y * (0.5 + 0.5 * reach)
            width: shadow.width + spread * 2
            height: shadow.height + spread * 2
            radius: shadow.radius + spread
            color: shadow.color
            opacity: shadow.level.strength / shadow.layers
            antialiasing: true
        }
    }
}
