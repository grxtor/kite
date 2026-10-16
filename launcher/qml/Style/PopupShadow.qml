// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

// A soft drop shadow for a rounded popup/menu/dialog surface, built from two
// oversized, low-opacity rounded rects rather than a blur -- this style's Qt
// floor (6.4) has neither MultiEffect nor ShaderEffect to draw a real one.
// Place as a child of the surface Rectangle with `radius` matching it.
// Children paint over their parent, so z is -1: without it the two scrim
// rects darkened the surface itself (a dark dialog drew #252525 instead of
// surfaceOverlay), and a ScrollFade fading into surfaceOverlay showed a band.
Item {
    id: shadow

    // Radius of the surface this sits behind, kept in sync so the shadow's
    // rounding never mismatches the card's own.
    property real radius: 0

    z: -1
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        anchors.margins: -10
        anchors.topMargin: -4
        anchors.bottomMargin: -14
        radius: shadow.radius + 8
        color: Theme.palette.scrim
        opacity: 0.14
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -4
        anchors.topMargin: -1
        anchors.bottomMargin: -6
        radius: shadow.radius + 3
        color: Theme.palette.scrim
        opacity: 0.20
    }
}
