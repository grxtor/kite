// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A quiet block-grid wash for a chrome-only screen's header region -- one
 * with no per-item cover art of its own to bleed behind it the way
 * LibraryPage/PlayDock do (see CoverArt.qml). Settings and Discover before
 * results would sit on flat canvas otherwise (design-plan.md §5
 * "chrome-only screens"); PlayDock uses it when its instance has no art.
 * Needs a band a few rows tall -- at a dialog header's height it is one or
 * two rows of dots, which read as a dotted rule rather than a wash.
 *
 * Painted once onto a Canvas rather than as a grid of Rectangle nodes, so it
 * costs one texture upload and nothing afterwards -- no Infinite animation,
 * matching Theme.motion's contract that a loop only ever runs for a real
 * busy/live state (see design-plan.md §2.3). `tint` defaults to the page's
 * own primary text colour, which is already correct in both themes (light
 * dots on dark canvas, dark dots on light canvas) without a second asset.
 */
Item {
    id: root

    property color tint: Theme.palette.textPrimary
    property real strength: Theme.dark ? 0.05 : 0.07
    // For a band that stops short of its surface's bottom (a page's header
    // region): rows thin out to nothing by the last one instead of the
    // field ending on a hard line.
    property bool fadeBottom: false
    readonly property int cell: 26
    readonly property int dot: 3

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Cooperative

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cols = Math.ceil(width / root.cell) + 1
            var rows = Math.ceil(height / root.cell) + 1
            for (var r = 0; r < rows; ++r) {
                var falloff = root.fadeBottom && rows > 1 ? 1 - r / (rows - 1) : 1
                ctx.fillStyle = Qt.rgba(root.tint.r, root.tint.g, root.tint.b, root.strength * falloff)
                // Staggered every other row -- a blocky, brick-like field
                // rather than a perfect graph-paper grid.
                var offset = (r % 2) * (root.cell / 2)
                for (var c = 0; c < cols; ++c)
                    ctx.fillRect(c * root.cell + offset, r * root.cell, root.dot, root.dot)
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    onTintChanged: canvas.requestPaint()
    onStrengthChanged: canvas.requestPaint()
    onFadeBottomChanged: canvas.requestPaint()
}
