// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

// Every interactive control in this style draws its keyboard focus ring the
// same way, so it lives here once instead of being re-typed in twenty control
// files. Bind `visible` to `control.visualFocus` -- never `activeFocus` --
// because visualFocus is false for a focus gained by mouse/touch press and
// true only for keyboard/programmatic focus. That is what keeps the ring from
// flashing on every click, which is the usual complaint about focus rings.
//
// Place it as a child of the control's background Rectangle: it follows
// that shape on its own, standing Theme.focus.offset clear of it. A text
// field sets `halo` instead and shows it on any focus: the same accent,
// softened to Theme.focus.haloOpacity and hugging the field's own edge.
Rectangle {
    id: ring

    property bool halo: false
    // The corner of the shape this ring goes around; the parent's own when
    // it has one.
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

    // The ring fades in/out with the rest of the style's state changes rather
    // than snapping, so tabbing through a form does not feel jumpy.
    Behavior on opacity {
        NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    opacity: visible ? 1 : 0
}
