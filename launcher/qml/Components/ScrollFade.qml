// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The bottom edge of a scrolling page: while there is more content below the
 * fold, the last visible rows fade into the canvas. Place it as a sibling after
 * the Flickable and point `flickable` at it; it spans its parent's width along the
 * bottom edge (the top with `atTop`). Set the anchors again for any other place.
 */
Rectangle {
    id: root

    property Flickable flickable: null
    // What the content fades into: the page background; over the Gece sky, the sky colour at about
    // where the edge sits.
    property color fadeTo: Theme.look === "night" ? Theme.sky.colorAt(root.atTop ? root.skyTop : root.skyBottom)
                                                  : Theme.palette.canvas
    readonly property real skyTop: 0.1
    readonly property real skyBottom: 0.9
    // The top edge instead, while content is scrolled up above it; anchor it to the Flickable's top.
    property bool atTop: false

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: root.atTop ? parent.top : undefined
    anchors.bottom: root.atTop ? undefined : parent.bottom
    height: Theme.space.xxl
    // From originY, not 0: a view with a header starts its content above zero, and the fade stayed
    // on with everything in view.
    visible: !!root.flickable
             && (root.atTop ? root.flickable.contentY - root.flickable.originY > 1
                          : root.flickable.contentY - root.flickable.originY + root.flickable.height
                            < root.flickable.contentHeight - 1)
    gradient: Gradient {
        GradientStop { position: 0.0; color: root.atTop ? root.fadeTo : Qt.rgba(root.fadeTo.r, root.fadeTo.g, root.fadeTo.b, 0) }
        GradientStop { position: 1.0; color: root.atTop ? Qt.rgba(root.fadeTo.r, root.fadeTo.g, root.fadeTo.b, 0) : root.fadeTo }
    }
}
