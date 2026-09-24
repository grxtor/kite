// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One tile in the Home page's "Recent worlds" row: a world's own icon (or,
 * lacking one, one of PixelArt's 16x16 block textures), the world's name,
 * which instance it belongs to, and when it was last played. Used directly
 * as a Repeater delegate against the shell's recentWorlds model, so the
 * required properties are filled from RecentWorldsModel's roles.
 */
AbstractButton {
    id: control

    required property string worldName
    required property string folderName
    required property string iconUrl
    required property var lastPlayed
    required property string instanceId
    required property string instanceName

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
            // 48 = three whole pixels per texel of a 16x16 block texture,
            // so the fallback below stays crisp rather than unevenly scaled.
            width: 48
            height: 48
            radius: Theme.radius.sm
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

            // No world icon: one of PixelArt's block textures, picked
            // deterministically by this world's own folder name -- a
            // bounded, designed fallback in place of the old flat
            // surfaceOverlay square (design-plan.md G3). A whole 96x54
            // landscape scene would only be an unreadable smear at this
            // size; a single block reads at a glance. No sourceSize: the
            // texture is 16x16 and must scale by nearest neighbour, not be
            // pre-filtered.
            Image {
                anchors.fill: parent
                visible: !control.hasIcon
                source: control.hasIcon ? "" : PixelArt.blockUrlFor(control.folderName)
                fillMode: Image.Stretch
                smooth: false
                asynchronous: true
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
