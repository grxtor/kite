// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A group's title row: name, an optional item count, and -- when the group
 * can fold -- a chevron after them that turns to show the collapsed state.
 * The same look as the library's own group headers (InstanceSection), so a
 * titled group reads the same on every page.
 */
Item {
    id: root

    property string title: ""
    property int count: -1
    property bool collapsed: false
    property bool collapsible: true

    implicitHeight: Theme.control.heightSm
    implicitWidth: row.implicitWidth

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        spacing: Theme.space.sm

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Font.Bold
        }

        Tag {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.count >= 0
            text: root.count
        }

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.collapsible
            iconName: "chevron-down"
            size: Theme.icon.sm
            color: Theme.palette.textTertiary
            rotation: root.collapsed ? -90 : 0

            Behavior on rotation { RotationAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.collapsible
        cursorShape: root.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.collapsed = !root.collapsed
    }
}
