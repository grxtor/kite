// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import MeshMC.Theme

/*
 * A quiet block-grid wash for a chrome-only screen's header region -- one
 * with no per-item cover art of its own to bleed behind it the way
 * LibraryPage/PlayDock do (see CoverArt.qml). Settings and Discover before
 * results would sit on flat canvas otherwise (design-plan.md §5
 * "chrome-only screens"), and the New instance dialog's header.
 * Needs a band a few rows tall; a dialog header's short one still works
 * once it fades early (fadeStart) and at its sides (fadeSides).
 *
 * Actually tiles art/ambient/blocks_mask.png -- a dirt/deepslate
 * checkerboard from the same generator that draws G3a's block textures,
 * pre-converted (generate.py's ambient_tile()) into a luminance alpha mask
 * -- rather than an unrelated abstract motif; G3b asked for "a subtle tiled
 * block texture (e.g. dirt or deepslate, or a mix)", and a disconnected
 * Canvas pattern was a drift from that brief. IconImage recolours it via
 * its alpha channel, the exact mechanism MeshIcon.qml already uses for
 * every SVG icon in the app (see its own file comment) -- no
 * ShaderEffect/MultiEffect, so it works below the Qt 6.5 floor this
 * codebase otherwise avoids (see CoverArt.qml/PopupShadow.qml). `tint`
 * defaults to the page's own primary text colour, already correct in both
 * themes without a second asset; `strength` rides the colour's own alpha,
 * so no per-frame drawing cost -- one static, non-animated texture upload,
 * matching Theme.motion's contract that a loop only ever runs for a real
 * busy/live state (design-plan.md §2.3).
 */
Item {
    id: root

    property color tint: Theme.palette.textPrimary
    property real strength: Theme.dark ? 0.05 : 0.07
    // For a band that stops short of its surface's bottom (a page's header
    // region): the wash fades into `fadeColor` instead of ending on a hard
    // line -- the same idiom PageBackdrop.qml's own fadeBottom uses.
    property bool fadeBottom: false
    // Where, as a fraction of the height, fadeBottom starts to fade. A tall
    // page band keeps most of its wash and only thins at the foot; a short
    // dialog header needs to start sooner to reach the surface by its edge.
    property real fadeStart: 0.7
    // Width, in px, of a fade into fadeColor along the left and right edges
    // (0 = none). For a band that sits inside a rounded surface and must not
    // end on a hard vertical line.
    property int fadeSides: 0
    // What fadeBottom/fadeSides fade into. A page-level caller (Settings,
    // Discover) sits directly on the page canvas, so painting an opaque wash
    // of this colour over the pattern's edge reads exactly like the pattern
    // thinning out into nothing there; a caller on some other surface (a
    // dialog's overlay colour) passes that surface's colour instead.
    property color fadeColor: Theme.palette.canvas
    readonly property int cell: 32

    IconImage {
        anchors.fill: parent
        source: PixelArt.ambientMaskUrl()
        sourceSize: Qt.size(root.cell, root.cell)
        fillMode: Image.Tile
        smooth: false
        color: Qt.rgba(root.tint.r, root.tint.g, root.tint.b, root.strength)
    }

    Rectangle {
        visible: root.fadeBottom
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.fadeColor.r, root.fadeColor.g, root.fadeColor.b, 0) }
            GradientStop { position: root.fadeStart; color: Qt.rgba(root.fadeColor.r, root.fadeColor.g, root.fadeColor.b, 0) }
            GradientStop { position: 1.0; color: root.fadeColor }
        }
    }

    Rectangle {
        visible: root.fadeSides > 0
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.fadeSides
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.fadeColor }
            GradientStop { position: 1.0; color: Qt.rgba(root.fadeColor.r, root.fadeColor.g, root.fadeColor.b, 0) }
        }
    }

    Rectangle {
        visible: root.fadeSides > 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.fadeSides
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(root.fadeColor.r, root.fadeColor.g, root.fadeColor.b, 0) }
            GradientStop { position: 1.0; color: root.fadeColor }
        }
    }
}
