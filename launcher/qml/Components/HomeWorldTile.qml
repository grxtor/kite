// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One world in the Home page's "Recent worlds": the same 80px row grammar
 * as "Jump back in" (HomeJumpCard) -- the world's own icon (or, lacking one,
 * a neutral globe on the calm surface, the Worlds tab's own fallback), the
 * world's name, and which instance it belongs to with when it was last
 * played. Used directly as a Repeater delegate against the shell's
 * recentWorlds model, so the required properties are filled from
 * RecentWorldsModel's roles.
 *
 * Clicking it opens that instance's Worlds tab. Hover lifts it like every
 * other card: one step lighter and a small shadow.
 */
AbstractButton {
    id: control

    required property string worldName
    required property string folderName
    required property string iconUrl
    required property var lastPlayed
    required property string instanceId
    required property string instanceName

    readonly property bool hasIcon: control.iconUrl.length > 0 && worldIcon.status !== Image.Error
    // The row and its icon square, as on "Jump back in".
    readonly property int rowHeight: 80
    readonly property int squareSize: 48

    implicitWidth: 320
    implicitHeight: control.rowHeight
    hoverEnabled: true
    leftPadding: Theme.space.lg
    rightPadding: Theme.space.lg

    Accessible.name: qsTr("%1, in %2").arg(control.worldName).arg(control.instanceName)

    ToolTip.visible: control.hovered && (nameText.truncated || metaText.truncated)
    ToolTip.delay: 500
    ToolTip.text: qsTr("%1 — %2").arg(control.worldName).arg(control.instanceName)

    background: Rectangle {
        id: card
        radius: Theme.radius.lg
        color: control.down ? Theme.palette.surfaceRaised
             : control.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        border.width: control.visualFocus ? Theme.focus.width : 1
        border.color: control.visualFocus ? Theme.palette.focusRing
                    : control.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: card.radius
            level: Theme.elevation.sm
            opacity: control.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }
    }

    contentItem: Item {
        Item {
            id: square
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: control.squareSize
            height: control.squareSize

            PlaceholderSurface {
                anchors.fill: parent
                radius: Theme.radius.md
            }
            // The game saves icon.png at 64x64; drawn a little smaller,
            // filtered so the downscale stays clean.
            Image {
                id: worldIcon
                anchors.fill: parent
                visible: control.hasIcon
                source: control.iconUrl
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
            }
            CornerMatte {
                visible: control.hasIcon
                cornerRadius: Theme.radius.md
                matte: card.color
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
            anchors.left: square.right
            anchors.leftMargin: Theme.space.md
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xxs

            Text {
                id: nameText
                width: parent.width
                text: control.worldName
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
            }

            // "Hardcore Survival • 1 h ago"
            Text {
                id: metaText
                width: parent.width
                text: [control.instanceName, Format.lastPlayed(control.lastPlayed)]
                      .filter((part) => part.length > 0).join("  •  ")
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }
    }
}
