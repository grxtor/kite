// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

// The keyboard focus ring every control in this style shares. Bind `visible`
// to `control.visualFocus`, never `activeFocus`: visualFocus is false for
// focus gained by a mouse or touch press, so the ring does not flash on every
// click.
//
// Place it as a child of the control's background Rectangle: it follows that
// shape, standing Theme.focus.offset clear of it. A text field sets `halo`
// instead and shows it on any focus: the same accent at
// Theme.focus.haloOpacity, hugging the field's edge.
Rectangle {
    id: ring

    property bool halo: false
    // The corner of the shape this ring goes around; the parent's when it has
    // one.
    property real baseRadius: parent && parent.radius !== undefined ? parent.radius : 0
    readonly property real reach: Theme.focus.width + (halo ? 0 : Theme.focus.offset)

    anchors.fill: parent
    anchors.margins: -reach
    radius: baseRadius + reach
    color: "transparent"
    border.width: Theme.focus.width
    border.color: halo ? Qt.rgba(Theme.palette.focusRing.r, Theme.palette.focusRing.g,
                                 Theme.palette.focusRing.b, Theme.focus.haloOpacity)
                       : Theme.palette.focusRing
    antialiasing: true

    // Fades with the style's other state changes instead of snapping.
    Behavior on opacity {
        NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    opacity: visible ? 1 : 0
}
