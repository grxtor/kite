// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

// Every interactive control in this style draws its keyboard focus ring the
// same way, so it lives here once instead of being re-typed in twenty control
// files. Bind `visible` to `control.visualFocus` -- never `activeFocus` --
// because visualFocus is false for a focus gained by mouse/touch press and
// true only for keyboard/programmatic focus. That is what keeps the ring from
// flashing on every click, which is the usual complaint about focus rings.
Rectangle {
    id: ring

    // Callers place this as a sibling on top of (or inset from) the control's
    // background and set radius/anchors to match its shape.
    color: "transparent"
    border.width: 2
    border.color: Theme.palette.focusRing
    antialiasing: true

    // The ring fades in/out with the rest of the style's state changes rather
    // than snapping, so tabbing through a form does not feel jumpy.
    Behavior on opacity {
        NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    opacity: visible ? 1 : 0
}
