// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The placeholder player figure: shown under a skin render while it loads,
 * or in place of one while there is no texture at all yet (an account
 * without a skin of its own wears the game's default once that has
 * downloaded -- see DefaultSkins -- so only before that). Drawn in the
 * game's own proportions -- an 8x8 head over an 8x12 torso, 4x12 arms at its
 * sides and two 4x12 legs, 16x32 in all, the same frame
 * AccountFaceProvider's body render fills -- so a real skin lands exactly on
 * top of it.
 *
 * Neutral greys rather than an account tint: one quiet figure that reads as
 * "a player" in either theme. The parts step down in strength from the
 * head, the way the front of a lit figure does, so it reads as a body
 * rather than a flat cut-out.
 *
 * Size it like the render it stands in for: any width x height of 1:2.
 */
Item {
    id: root

    property color color: Theme.palette.textDisabled

    implicitWidth: 64
    implicitHeight: 128

    // One skin pixel, in item pixels.
    readonly property real unit: Math.min(width / 16, height / 32)
    readonly property real torsoOpacity: 0.85
    readonly property real armOpacity: 0.7
    readonly property real legOpacity: 0.6

    // The 16x32 frame, centred in whatever box the caller gives.
    Item {
        anchors.centerIn: parent
        width: 16 * root.unit
        height: 32 * root.unit

        // Head
        Rectangle {
            x: 4 * root.unit
            width: 8 * root.unit
            height: 8 * root.unit
            radius: Theme.radius.xs
            color: root.color
        }
        // Torso
        Rectangle {
            x: 4 * root.unit
            y: 8 * root.unit
            width: 8 * root.unit
            height: 12 * root.unit
            color: root.color
            opacity: root.torsoOpacity
        }
        // Arms, either side of the torso.
        Repeater {
            model: [0, 12]
            Rectangle {
                required property int modelData
                x: modelData * root.unit
                y: 8 * root.unit
                width: 4 * root.unit
                height: 12 * root.unit
                color: root.color
                opacity: root.armOpacity
            }
        }
        // Legs, with a thin seam between them.
        Repeater {
            model: [4, 8]
            Rectangle {
                required property int modelData
                x: modelData * root.unit + (modelData === 8 ? root.unit / 4 : 0)
                y: 20 * root.unit
                width: 4 * root.unit - root.unit / 4
                height: 12 * root.unit
                color: root.color
                opacity: root.legOpacity
            }
        }
    }
}
