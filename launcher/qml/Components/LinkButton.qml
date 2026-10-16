// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A section header's trailing action ("See all", "View all", "Manage"): text
 * with a small chevron and no padding of its own, so its last glyph lines up
 * with the content edge below it. A flat Button carries a hover background
 * and therefore horizontal padding, which left these actions visibly inset
 * from the cards' right edge. Hover changes only the colour.
 */
AbstractButton {
    id: control

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(row.implicitHeight, Theme.control.heightSm)

    Accessible.role: Accessible.Link
    Accessible.name: control.text

    readonly property color ink: control.hovered || control.visualFocus ? Theme.palette.textPrimary
                                                                         : Theme.palette.textSecondary

    contentItem: Item {}

    // chevron-right's glyph sits in the middle of its square; this much of
    // the square to its right is empty, so the row is pulled out by it to put
    // the glyph's own edge on the content edge.
    readonly property real chevronBearing: Theme.icon.sm * 0.34

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -control.chevronBearing
        spacing: Theme.space.xxs

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.ink
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Theme.type.label.weight
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: control.ink
        }
    }

    // Keyboard focus: an underline rather than a ring, the way links show it.
    Rectangle {
        anchors.left: row.left
        anchors.right: row.right
        anchors.top: row.bottom
        height: 1
        color: Theme.palette.focusRing
        visible: control.visualFocus
    }
}
