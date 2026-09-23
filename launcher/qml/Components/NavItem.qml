// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One destination in the sidebar. The selected fill itself is a single
 * shared pill SidebarNav slides between items (see its activeIndicator) --
 * this control only ever draws its own transient hover/press state, so the
 * pill shows through untouched wherever it sits.
 */
AbstractButton {
    id: control

    property string iconName
    property string label
    property bool selected: false
    // Icon rail mode: centers the icon and drops the label, which shows as
    // a tooltip instead.
    property bool collapsed: false

    implicitHeight: Theme.control.height + Theme.space.xs
    implicitWidth: 200
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: label

    ToolTip.visible: control.collapsed && control.hovered
    ToolTip.delay: 400
    ToolTip.text: control.label

    // Collapsed: a compact square hugging just the icon rather than the
    // whole stretched row -- the same square SidebarNav's own activeIndicator
    // uses for the selected item (see its own comment), so hover and
    // selection read as the same shape in the rail.
    readonly property int railSquare: 40

    background: Rectangle {
        width: control.collapsed ? control.railSquare : control.width
        height: control.collapsed ? control.railSquare : control.height
        x: control.collapsed ? (control.width - width) / 2 : 0
        y: control.collapsed ? (control.height - height) / 2 : 0
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

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

    contentItem: Item {
        Row {
            id: iconRow
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: control.collapsed ? undefined : parent.left
            anchors.horizontalCenter: control.collapsed ? parent.horizontalCenter : undefined
            leftPadding: control.collapsed ? 0 : Theme.space.md
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
                visible: !control.collapsed
                text: control.label
                color: control.selected || control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: control.selected ? Font.DemiBold : Font.Medium
            }
        }
    }
}
