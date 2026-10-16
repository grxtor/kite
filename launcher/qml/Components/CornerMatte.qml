// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Rounds the corners of an image in a rounded frame. Rectangle's `clip`
 * only clips to the bounding box, so a picture inside a rounded frame keeps
 * its square corners; this ring -- the same trick CoverArt uses -- is drawn
 * in `matte`, the colour of whatever is behind the frame, and covers exactly
 * the four corners outside the rounded outline. Place it as the frame's last
 * child (after the image) and give it the frame's radius and background;
 * `outline` redraws the frame's hairline over the picture's edge.
 */
Rectangle {
    id: root

    property color matte: Theme.palette.canvas
    property int cornerRadius: Theme.radius.lg
    property color outline: "transparent"
    // Wide enough to cover a corner of any radius on the scale.
    readonly property int ring: root.cornerRadius + Theme.space.xxs

    anchors.fill: parent
    anchors.margins: -root.ring
    radius: root.cornerRadius + root.ring
    color: "transparent"
    border.width: root.ring
    border.color: root.matte

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.ring
        radius: root.cornerRadius
        color: "transparent"
        border.width: 1
        border.color: root.outline
        visible: root.outline.a > 0
    }
}
