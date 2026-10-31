// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A shimmering placeholder block for content still loading: a flat tinted
 * rectangle with a highlight band sweeping across it. No shader (the Qt floor
 * is 6.4): the shine is a horizontal Gradient translated by a NumberAnimation.
 */
Item {
    id: root

    property int radius: Theme.radius.md

    // Decorative loops stop when the window is inactive or "Reduce motion" is on.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive
                                          && !SettingsStore.bool("UiReduceMotion")

    clip: true

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        // Raised in dark mode: surfaceSunken is near-black there and a loading card read as a hole.
        color: Theme.dark ? Theme.palette.surfaceRaised : Theme.palette.surfaceSunken
    }

    Rectangle {
        id: sweep
        visible: !SettingsStore.bool("UiReduceMotion")
        width: Math.max(1, root.width) * 0.4
        height: parent.height
        radius: root.radius
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0) }
            GradientStop { position: 0.5; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0.85) }
            GradientStop { position: 1.0; color: Qt.rgba(Theme.palette.surfaceOverlay.r, Theme.palette.surfaceOverlay.g, Theme.palette.surfaceOverlay.b, 0) }
        }

        SequentialAnimation on x {
            loops: Animation.Infinite
            running: root.visible && root.motionEnabled
            NumberAnimation { from: -sweep.width; to: root.width; duration: 1300; easing.type: Easing.InOutSine }
            PauseAnimation { duration: 450 }
        }
    }
}
