// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Rounds the corners of an image in a rounded frame (as CoverArt does): a ring in `matte`, the colour behind the
 * frame, covers the four corners, since Rectangle's `clip` only clips to the bounding box. Place it as the frame's
 * last child with the frame's radius and background; `outline` redraws the hairline. The ring is clipped to the
 * frame, so a frame close to its card's edge does not paint over the card's border or past it.
 */
Item {
    id: root

    property color matte: Theme.palette.canvas
    property int cornerRadius: Theme.radius.lg
    property color outline: "transparent"
    readonly property int ring: root.cornerRadius + Theme.space.xxs

    anchors.fill: parent
    clip: true

    Rectangle {
        anchors.fill: parent
        anchors.margins: -root.ring
        radius: root.cornerRadius + root.ring
        color: "transparent"
        border.width: root.ring
        border.color: root.matte
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        border.width: 1
        border.color: root.outline
        visible: root.outline.a > 0
    }
}
