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
    // Icon rail mode: drops the label, which shows as a tooltip instead.
    // The icon itself never moves: it sits on the same axis in both modes
    // (iconAxis), and the collapsed rail is exactly twice that wide
    // (SidebarNav.railWidth), so it is centred there without re-anchoring
    // anything -- swapping anchors at runtime left the icon behind.
    property bool collapsed: false
    // The icon's centre, from the row's left edge.
    readonly property int iconAxis: Theme.space.md + Theme.icon.md / 2

    implicitHeight: Theme.control.height + Theme.space.xs
    implicitWidth: 200
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.PageTab
    Accessible.name: label

    ToolTip.visible: control.collapsed && control.hovered
    ToolTip.delay: 400
    ToolTip.text: control.label

    // Collapsed: a compact square centred on the icon rather than the
    // whole stretched row -- the same square SidebarNav's selection
    // indicator draws for the selected item (NavSelectionIndicator), so
    // hover and selection read as the same shape in the rail.
    readonly property int railSquare: 40

    background: Rectangle {
        width: control.collapsed ? control.railSquare : control.width
        height: control.collapsed ? control.railSquare : control.height
        x: control.collapsed ? control.iconAxis - width / 2 : 0
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
            anchors.left: parent.left
            leftPadding: control.iconAxis - Theme.icon.md / 2
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
