// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Frosted glass for the Gece look, where Home floats over the night sky: a faint white fill and edge, lit a step
 * when its host is hovered. No blur (Qt 6.4 has no MultiEffect). The Sahne stage's world panel is the same in
 * black (`dark`).
 */
Rectangle {
    property bool lit: false
    property bool dark: false

    readonly property color restFill: dark ? Qt.rgba(0.04, 0.04, 0.05, 0.42) : Qt.rgba(1, 1, 1, 0.06)
    readonly property color litFill: dark ? Qt.rgba(0.04, 0.04, 0.05, 0.55) : Qt.rgba(1, 1, 1, 0.11)

    radius: Theme.radius.lg
    color: lit ? litFill : restFill
    border.width: dark ? 0 : 1
    border.color: Qt.rgba(1, 1, 1, 0.09)

    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
}
