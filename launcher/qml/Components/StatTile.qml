// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

// One number worth seeing at a glance: a label over a big value.
Item {
    id: root

    property string label
    property string value
    property string iconName

    implicitWidth: 180
    implicitHeight: 88

    // A faint duplicate a few pixels below the card reads as a soft drop
    // shadow without a real blur (none of the effect modules are available
    // at this Qt floor).
    Rectangle {
        x: 0
        y: 3
        width: parent.width
        height: parent.height
        radius: Theme.radius.lg
        color: Theme.palette.scrim
        opacity: 0.08
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.sm

        Row {
            spacing: Theme.space.xs + 2

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName.length > 0
                width: Theme.icon.md + Theme.space.xs
                height: width
                radius: Theme.radius.sm + 2
                color: Theme.palette.accentSubtle
                MeshIcon {
                    anchors.centerIn: parent
                    iconName: root.iconName
                    size: Theme.icon.sm
                    color: Theme.palette.accentText
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.Medium
            }
        }

        Text {
            width: parent.width
            text: root.value.length > 0 ? root.value : "—"
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.heading.pixelSize
            font.weight: Font.Bold
        }
    }
}
