// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One tile in the Home page's "Recent worlds" row: a world's own icon (or,
 * lacking one, a neutral block tile with its instance's icon), the world's
 * name, which instance it belongs to, and when it was last played. Used
 * directly as a Repeater delegate against the shell's recentWorlds model,
 * so the required properties are filled from RecentWorldsModel's roles.
 */
AbstractButton {
    id: control

    required property string worldName
    required property string folderName
    required property string iconUrl
    required property var lastPlayed
    required property string instanceId
    required property string instanceName
    required property string instanceIconKey

    readonly property bool hasIcon: control.iconUrl.length > 0

    implicitWidth: 176
    implicitHeight: 72
    hoverEnabled: true

    Accessible.name: qsTr("%1, in %2").arg(control.worldName).arg(control.instanceName)

    ToolTip.visible: control.hovered
    ToolTip.delay: 500
    ToolTip.text: qsTr("%1 — %2").arg(control.worldName).arg(control.instanceName)

    background: Rectangle {
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        border.width: 1
        border.color: Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        Rectangle {
            id: tile
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            radius: Theme.radius.sm
            // A neutral plate, not a per-world tint: this is a small
            // fallback tile, not the cover art treatment CoverArt gives a
            // whole card (design-plan.md §2.9/§6 -- a bounded, designed
            // colour, never a giant centred glyph on a random hue).
            color: control.hasIcon ? "transparent" : Theme.palette.surfaceOverlay
            border.width: 1
            border.color: Theme.palette.border
            clip: true

            Image {
                anchors.fill: parent
                visible: control.hasIcon
                source: control.hasIcon ? control.iconUrl : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectCrop
                smooth: false
                asynchronous: true
            }

            Image {
                anchors.centerIn: parent
                visible: !control.hasIcon
                width: 26
                height: 26
                source: control.instanceIconKey.length > 0 ? "image://instanceicon/" + control.instanceIconKey : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                smooth: false
            }
        }

        Column {
            anchors.left: tile.right
            anchors.leftMargin: Theme.space.sm
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xxs

            Text {
                width: parent.width
                text: control.worldName
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Theme.type.label.weight
            }

            Text {
                width: parent.width
                text: control.instanceName
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            Text {
                width: parent.width
                text: Format.lastPlayed(control.lastPlayed)
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }
}
