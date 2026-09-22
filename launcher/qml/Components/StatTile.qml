// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

// One number worth seeing at a glance: a label over a big value.
Rectangle {
    id: root

    property string label
    property string value
    property string iconName

    implicitWidth: 180
    implicitHeight: 84
    radius: Theme.radius.lg
    color: Theme.palette.surface
    border.width: 1
    border.color: Theme.palette.border

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.xs

        Row {
            spacing: Theme.space.xs + 2
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName.length > 0
                iconName: root.iconName
                size: Theme.icon.sm
                color: Theme.palette.textTertiary
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
