// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The bottom edge of a scrolling page: while there is more content below the
 * fold, the last visible rows fade into the canvas instead of being sliced
 * mid-line (or mid-icon) by the play bar or the window edge -- the cue that
 * the page scrolls, at every window height. Place it as a sibling after the
 * Flickable, anchored to the same bottom edge, and point `flickable` at it.
 */
Rectangle {
    id: root

    // May be null for a moment while the page is still being built.
    property Flickable flickable: null
    // What the content fades into; the page's own background.
    property color fadeTo: Theme.palette.canvas
    // The top edge instead, while there is content scrolled up above it:
    // anchor it to the Flickable's top. For a list whose rows are cut by a
    // border at both ends.
    property bool atTop: false

    height: Theme.space.xxl
    // From originY, not 0: a ListView or GridView with a header starts its
    // content above zero, and measured from zero the fade stayed on even
    // with everything in view.
    visible: !!root.flickable
             && (root.atTop ? root.flickable.contentY - root.flickable.originY > 1
                          : root.flickable.contentY - root.flickable.originY + root.flickable.height
                            < root.flickable.contentHeight - 1)
    gradient: Gradient {
        GradientStop { position: 0.0; color: root.atTop ? root.fadeTo : Qt.rgba(root.fadeTo.r, root.fadeTo.g, root.fadeTo.b, 0) }
        GradientStop { position: 1.0; color: root.atTop ? Qt.rgba(root.fadeTo.r, root.fadeTo.g, root.fadeTo.b, 0) : root.fadeTo }
    }
}
