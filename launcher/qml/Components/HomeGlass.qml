// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Frosted glass for the Gece look, where Home floats over the night sky
 * instead of sitting on a page: a faint white fill and a faint white edge,
 * lit a step when its host is hovered. There is no blur behind it (the 6.4
 * floor has no MultiEffect), so it is only as frosted as a translucent fill
 * can be. The Sahne stage's world panel is the same idea in black
 * (`dark`).
 */
Rectangle {
    // Hovered or otherwise raised.
    property bool lit: false
    // A dark pane (over a photo) instead of a light one (over the sky).
    property bool dark: false

    readonly property color restFill: dark ? Qt.rgba(0.04, 0.04, 0.05, 0.42) : Qt.rgba(1, 1, 1, 0.06)
    readonly property color litFill: dark ? Qt.rgba(0.04, 0.04, 0.05, 0.55) : Qt.rgba(1, 1, 1, 0.11)

    radius: Theme.radius.lg
    color: lit ? litFill : restFill
    border.width: dark ? 0 : 1
    border.color: Qt.rgba(1, 1, 1, 0.09)

    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
}
