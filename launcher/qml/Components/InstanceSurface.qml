// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The card behind an instance in the library, as a tile or as a row: surface that lifts under the pointer and when
 * selected, the hover shadow, a hairline that strengthens with hover and selection, and the keyboard focus ring.
 * The host (InstanceCard, InstanceListRow) sets `hovered`, `selected` and `focused` and answers the three taps.
 */
StateFill {
    id: root

    property bool selected: false
    // The host has keyboard focus; the ring stays off while it is selected.
    property bool focused: false

    signal clicked()
    signal doubleClicked()
    signal contextRequested()

    card: true
    active: root.selected
    radius: Theme.radius.lg

    Shadow {
        radius: root.radius
        level: Theme.elevation.sm
        opacity: root.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    // ReleaseWithinBounds grabs on press, so the page's click-to-deselect handler never sees a card click.
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
        onDoubleTapped: root.doubleClicked()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.contextRequested()
    }

    // Over the content, so a picture's corner matte never paints across the hairline.
    Rectangle {
        anchors.fill: parent
        z: 1
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: root.selected || root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // Outset so it never competes with the selected state.
    FocusFrame { z: 1; visible: root.focused && !root.selected }
}
