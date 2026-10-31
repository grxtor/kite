// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The placeholder player figure, shown under a skin render while it loads or
 * while there is no texture yet. Drawn in the game's proportions (8x8 head,
 * 8x12 torso, 4x12 arms and legs; 16x32 in all, the frame AccountFaceProvider's
 * body render fills) so a real skin lands exactly on top. Size it 1:2.
 */
Item {
    id: root

    property color color: Theme.palette.textDisabled

    implicitWidth: 64
    implicitHeight: 128

    readonly property real unit: Math.min(width / 16, height / 32)
    readonly property real torsoOpacity: 0.85
    readonly property real armOpacity: 0.7
    readonly property real legOpacity: 0.6

    Item {
        anchors.centerIn: parent
        width: 16 * root.unit
        height: 32 * root.unit

        Rectangle {
            x: 4 * root.unit
            width: 8 * root.unit
            height: 8 * root.unit
            radius: Theme.radius.xs
            color: root.color
        }
        Rectangle {
            x: 4 * root.unit
            y: 8 * root.unit
            width: 8 * root.unit
            height: 12 * root.unit
            color: root.color
            opacity: root.torsoOpacity
        }
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
