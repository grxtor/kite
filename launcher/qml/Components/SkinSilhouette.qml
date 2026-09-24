// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A neutral player figure (head, body, two legs) shown under a skin render
 * while it loads, or in place of one for an account with no skin -- tinted
 * per account by the caller (design-plan.md §9). `unit` scales the whole
 * figure; at 1 it is 64x162.
 */
Item {
    id: root

    property color color: Theme.palette.textTertiary
    property real unit: 1

    implicitWidth: 64 * root.unit
    implicitHeight: 162 * root.unit

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        width: 30 * root.unit
        height: width
        radius: width / 2
        color: root.color
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 34 * root.unit
        width: 42 * root.unit
        height: 58 * root.unit
        radius: Theme.radius.lg
        color: root.color
    }
    Repeater {
        // Left and right leg, offset from the figure's centre line.
        model: [-19, 3]
        Rectangle {
            required property int modelData
            x: root.width / 2 + modelData * root.unit
            y: 94 * root.unit
            width: 16 * root.unit
            height: 68 * root.unit
            radius: Theme.radius.sm
            color: root.color
        }
    }
}
