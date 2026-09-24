// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A page-level wash of one instance's own cover art -- what Library and Home
 * put behind their content so the canvas is not flat (design-plan.md §5/§6).
 * Uses CoverArt's own generated fallback plate when there is no screenshot
 * yet, so it reads as MeshMC's own content, never stock art.
 *
 * The caller positions and sizes it and decides when it is visible; this
 * only owns how strong the wash is, and -- when it does not reach the page's
 * bottom -- how it fades out instead of ending on a hard edge.
 */
Item {
    id: root

    property string cover: ""
    property color tint: Theme.palette.textTertiary
    // Set when the backdrop is a band (Home's top region) rather than the
    // whole page, so its lower edge dissolves into the canvas.
    property bool fadeBottom: false

    // A real screenshot stays inside the 6-10% design-plan.md §6 sets for a
    // background wash, biased to the top of that budget: at the low end it
    // was confirmed to sit at the edge of visible in an actual screenshot,
    // barely distinguishable from noise. CoverArt's fallback plate is a
    // smoother, lower-contrast gradient, so it needs a little more still to
    // read as tonal variation at all. Cover art is mostly dark, so on the
    // light canvas the same amount reads as a grey cast rather than a wash
    // -- scaled down there.
    readonly property real photoOpacity: Theme.dark ? 0.15 : 0.095
    readonly property real plateOpacity: Theme.dark ? 0.18 : 0.12

    clip: true

    Item {
        anchors.fill: parent
        opacity: root.cover.length > 0 ? root.photoOpacity : root.plateOpacity

        CoverArt {
            anchors.fill: parent
            source: root.cover
            tint: root.tint
            iconKey: ""
            radius: 0
            scrim: "none"
            // A real screenshot's hard edges (rooflines, tree cover) turn
            // blotchy at wash opacity when decoded sharp; the fallback plate
            // is already a smooth gradient and does not need this.
            photoSoftness: root.cover.length > 0 ? 0.12 : 1.0
        }
    }

    // Outside the faded Item on purpose: Qt Quick applies opacity per child,
    // so a canvas-coloured overlay inside it would only cover its own few
    // percent of the art. Drawn at full strength here it replaces the wash
    // with the canvas itself by the bottom edge.
    Rectangle {
        visible: root.fadeBottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height * 0.45
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(Theme.palette.canvas.r, Theme.palette.canvas.g, Theme.palette.canvas.b, 0) }
            GradientStop { position: 1.0; color: Theme.palette.canvas }
        }
    }
}
