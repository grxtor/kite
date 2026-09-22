// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A group's title row: name, an optional item count, and a chevron that
 * flips to show collapsed state. The chevron is drawn as rotated text
 * rather than an image asset, since no icon asset pipeline exists yet.
 */
Item {
    id: root

    property string title: ""
    // -1 hides the count pill entirely, so callers that don't have a
    // meaningful count (or don't want one shown) don't have to fake a zero.
    property int count: -1
    property bool collapsed: false
    property bool collapsible: true

    implicitHeight: Theme.control.heightSm
    implicitWidth: row.implicitWidth

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        spacing: Theme.space.xs

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "❯" // "❯"
            color: Theme.palette.textTertiary
            font.pixelSize: Theme.type.caption.pixelSize
            visible: root.collapsible
            rotation: root.collapsed ? 0 : 90

            Behavior on rotation { RotationAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Theme.type.label.weight
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.count >= 0
            text: root.count
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.collapsible
        cursorShape: root.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.collapsed = !root.collapsed
    }
}
