// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A group's title row: name, an optional item count and, when the group can fold,
 * a chevron that turns to show the collapsed state. A page-level section title
 * in Theme's heading size; InstanceSection's group headers stay at title size.
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

        Copy {
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            role: "heading"
        }

        Copy {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.count >= 0
            text: root.count
            role: "caption"
            tone: "tertiary"
            font.weight: Font.Medium
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
