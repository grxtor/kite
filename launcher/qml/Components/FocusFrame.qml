// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The keyboard focus ring of a card, row or tile: a Theme.focus.width border in palette.focusRing standing `around`
 * clear of `target`, following its corner. Shown while the target has keyboard focus (visualFocus for a control,
 * activeFocus for a plain Item); bind `visible` when the focus sits on another item. Not for Kite.Style's own
 * controls, which draw FocusRing.
 */
Rectangle {
    id: root

    property Item target: parent
    // How far outside the target the ring stands.
    property real around: Theme.space.xs
    // The target's corner, which the ring follows; a target without a radius is square.
    property real cornerRadius: target && target.radius !== undefined ? target.radius : 0

    anchors.fill: target
    anchors.margins: -around
    radius: Math.max(0, cornerRadius + around)
    color: "transparent"
    border.width: Theme.focus.width
    border.color: Theme.palette.focusRing
    visible: !!target && (target.visualFocus !== undefined ? target.visualFocus : target.activeFocus)
}
