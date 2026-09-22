// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * One entry in SidebarNav. icon is a short glyph string rather than an
 * image source: there is no icon asset pipeline yet, and a glyph lets
 * SidebarNav and Gallery supply sample data without one.
 */
Item {
    id: root

    property string icon: ""
    property string label: ""
    property bool selected: false
    // Set by SidebarNav when the rail has collapsed below its width
    // threshold; hides the label so only the glyph remains.
    property bool iconOnly: false

    signal clicked()

    implicitHeight: Theme.control.height
    implicitWidth: iconOnly ? implicitHeight : 200

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.pill
        color: root.selected ? Theme.palette.accentSubtle
                              : mouseArea.containsMouse ? Theme.palette.hoverOverlay
                                                         : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.md
        spacing: Theme.space.sm

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.icon
            color: root.selected ? Theme.palette.accentText : Theme.palette.textSecondary
            font.pixelSize: Theme.icon.md
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            visible: !root.iconOnly
            color: root.selected ? Theme.palette.accentText : Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Theme.type.label.weight
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
