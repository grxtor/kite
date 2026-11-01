// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Frosted glass for the Gece look, where Home floats over the night sky: a faint white fill and edge, lit a step
 * when its host is hovered. No blur (Qt 6.4 has no MultiEffect). The Sahne stage's world panel is the same in
 * black (`dark`).
 */
StateFill {
    property bool lit: false
    property bool dark: false

    active: lit
    rest: dark ? Theme.media.shade : Theme.media.glass
    activeFill: dark ? Theme.media.shadeLit : Theme.media.glassLit
    radius: Theme.radius.lg
    border.width: dark ? 0 : 1
    border.color: Theme.media.glassEdge
}
