// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One tile in the Home page's "Recent worlds" row: a world's own icon (or,
 * lacking one, a neutral globe -- the Worlds tab's own fallback), the world's name, which
 * instance it belongs to, and when it was last played. Used directly as a
 * Repeater delegate against the shell's recentWorlds model, so the required
 * properties are filled from RecentWorldsModel's roles.
 *
 * A filled surface tile with no outline; hover lifts only that fill to
 * surfaceRaised (Theme's motion contract: one property per hover).
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
    // The game saves a world's icon.png at 64x64, so a real icon draws 1:1
    // here; the tile's own height leaves an even md inset above and below.
    readonly property int iconSize: 64

    implicitWidth: 208
    implicitHeight: control.iconSize + Theme.space.md * 2
    hoverEnabled: true

    Accessible.name: qsTr("%1, in %2").arg(control.worldName).arg(control.instanceName)

    ToolTip.visible: control.hovered
    ToolTip.delay: 500
    ToolTip.text: qsTr("%1 — %2").arg(control.worldName).arg(control.instanceName)

    background: Rectangle {
        radius: Theme.radius.sm
        color: control.down ? Theme.palette.surfaceOverlay
             : control.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        Rectangle {
            id: tile
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            width: control.iconSize
            height: control.iconSize
            radius: Theme.radius.sm
            // Behind the icon while it decodes, and behind the globe.
            color: Theme.palette.surfaceSunken
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

            // No icon.png: a neutral globe, as on the instance's Worlds
            // tab. A landscape here would pass for a real view of the
            // world, and two worlds would share one.
            MeshIcon {
                anchors.centerIn: parent
                visible: !control.hasIcon
                iconName: "globe"
                size: Theme.icon.lg
                color: Theme.palette.textTertiary
            }
        }

        Column {
            anchors.left: tile.right
            anchors.leftMargin: Theme.space.sm
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.md
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
