// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The fill of something the pointer acts on, stepping up from rest to hover to press, and fading between them. A
 * plain control wears the overlays over its parent (palette.hoverOverlay, pressedOverlay); a `card` sits on
 * palette.surface and takes surfaceHover under the pointer. `active` (selected, current) wins over both. Set
 * radius and a border on it like on any Rectangle; keep the focus ring a separate FocusFrame.
 */
Rectangle {
    id: root

    property bool hovered: false
    property bool down: false
    property bool active: false
    property bool card: false
    // The fill at rest.
    property color rest: card ? Theme.palette.surface : "transparent"
    // The fill while `active`: a soft accent tint, or surfaceHover for a card.
    property color activeFill: card ? Theme.palette.surfaceHover : Theme.palette.accentSubtle

    color: active ? activeFill
         : card ? (hovered ? Theme.palette.surfaceHover : rest)
         : down ? Theme.palette.pressedOverlay
         : hovered ? Theme.palette.hoverOverlay : rest

    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
}
