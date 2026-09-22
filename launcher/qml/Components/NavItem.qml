// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One destination in the sidebar. Selected state is an accent bar at the
 * leading edge plus a raised fill -- the bar alone carries the meaning, so
 * it still reads when the fill is barely distinguishable from the rail.
 */
AbstractButton {
    id: control

    property string iconName
    property string label
    property bool selected: false

    implicitHeight: Theme.control.height + Theme.space.xs
    implicitWidth: 200
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: label

    background: Rectangle {
        radius: Theme.radius.md
        color: control.selected ? Theme.palette.surfaceRaised
             : control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3
            height: control.selected ? parent.height - Theme.space.md * 2 + 4 : 0
            radius: 2
            color: Theme.palette.accent
            Behavior on height { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            color: "transparent"
            border.width: 2
            border.color: Theme.palette.focusRing
            visible: control.visualFocus
        }
    }

    contentItem: Row {
        leftPadding: Theme.space.md
        spacing: Theme.space.md

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: control.iconName
            size: Theme.icon.md
            color: control.selected ? Theme.palette.accent
                 : control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.label
            color: control.selected || control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: control.selected ? Font.DemiBold : Font.Medium
        }
    }
}
