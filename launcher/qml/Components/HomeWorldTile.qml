// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One world in "Recent worlds" on Home's side panel: icon (a neutral globe without one), name, and its instance
 * with when it was last played. Clicking opens that instance's Worlds tab. On glass (`glass`) the row has no fill,
 * white words and square icon corners.
 */
AbstractButton {
    id: control

    required property string worldName
    required property string folderName
    required property string iconUrl
    required property var lastPlayed
    required property string instanceId
    required property string instanceName
    property bool glass: false

    readonly property bool compact: Theme.look !== "classic"
    readonly property int nameSize: control.compact ? Theme.type.label.pixelSize : Theme.type.bodyStrong.pixelSize
    readonly property int metaSize: control.compact ? Theme.type.caption.pixelSize : Theme.type.label.pixelSize
    readonly property color nameInk: control.glass ? Theme.media.text : Theme.palette.textPrimary
    readonly property color metaInk: control.glass ? Theme.media.textFaint : Theme.palette.textSecondary
    readonly property bool hasIcon: control.iconUrl.length > 0 && worldIcon.status !== Image.Error
    readonly property int squareSize: Theme.control.height
    readonly property int rowHeight: control.squareSize + Theme.space.sm * 2

    implicitWidth: 280
    implicitHeight: control.rowHeight
    hoverEnabled: true
    leftPadding: Theme.space.sm
    rightPadding: Theme.space.sm

    Accessible.name: qsTr("%1, in %2").arg(control.worldName).arg(control.instanceName)

    ToolTip.visible: control.hovered && (nameText.truncated || instanceText.truncated)
    ToolTip.delay: 500
    ToolTip.text: qsTr("%1 — %2").arg(control.worldName).arg(control.instanceName)

    background: Rectangle {
        id: card
        radius: Theme.radius.md
        color: control.glass ? "transparent" : Theme.palette.canvas

        StateFill {
            anchors.fill: parent
            radius: parent.radius
            hovered: control.hovered
            down: control.down
        }
        FocusFrame { around: 0; visible: control.visualFocus }
    }

    contentItem: Item {
        Item {
            id: square
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: control.squareSize
            height: control.squareSize

            // The game saves icon.png at 64x64 and shows it pixel-sharp: drawn at its own size, unfiltered.
            Image {
                id: worldIcon
                anchors.fill: parent
                visible: control.hasIcon
                source: control.iconUrl
                fillMode: Image.PreserveAspectCrop
                smooth: false
                asynchronous: true
            }
            // On glass no one colour lies around the icon, so a matte would show as dark corners.
            CornerMatte {
                visible: control.hasIcon && !control.glass
                cornerRadius: Theme.radius.md
                matte: card.color
            }
            // No icon.png: a neutral globe; a landscape would pass for a real view of the world and two worlds would
            // share one.
            MeshIcon {
                anchors.centerIn: parent
                visible: !control.hasIcon
                iconName: "globe"
                size: Theme.icon.md
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
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: control.nameInk
                font.family: Theme.font.family
                font.pixelSize: control.nameSize
                font.weight: Theme.type.bodyStrong.weight
            }

            // The instance's name gives way first, so the time stays whole.
            Row {
                id: metaRow
                width: parent.width

                Text {
                    id: instanceText
                    width: Math.max(0, Math.min(implicitWidth, metaRow.width - whenText.implicitWidth))
                    text: control.instanceName
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: control.metaInk
                    font.family: Theme.font.family
                    font.pixelSize: control.metaSize
                }
                Text {
                    id: whenText
                    readonly property string when: Format.lastPlayed(control.lastPlayed)
                    text: (control.instanceName.length > 0 && when.length > 0 ? "  •  " : "") + when
                    textFormat: Text.PlainText
                    color: control.metaInk
                    font.family: Theme.font.family
                    font.pixelSize: control.metaSize
                }
            }
        }
    }
}
