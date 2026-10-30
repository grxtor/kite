// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One world in "Recent worlds" on Home's side panel: a compact list row --
 * the world's own icon (or, lacking one, a neutral globe on the calm
 * surface, the Worlds tab's own fallback), the world's name, and which
 * instance it belongs to with when it was last played. Filled from the
 * shell's recentWorlds model (RecentWorldsModel's roles).
 *
 * Clicking it opens that instance's Worlds tab. A row in a list rather than
 * a card of its own: no fill or border at rest, the hover overlay under the
 * pointer.
 *
 * In the Sahne, Sade and Gece looks the text is a step smaller (as in their
 * mockups), and on glass (`glass`: the Sahne stage's world panel) the row
 * has no fill of its own and its words are white on whatever the glass is
 * over, and the icon keeps the game's square corners.
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
    readonly property color metaInk: control.glass ? Qt.rgba(1, 1, 1, 0.55) : Theme.palette.textSecondary
    readonly property bool hasIcon: control.iconUrl.length > 0 && worldIcon.status !== Image.Error
    // A compact row: a control-high icon square and a little air.
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

    // The panel's own colour under it at rest (the icon's corner matte
    // paints in it), the hover and press overlays over that.
    background: Rectangle {
        id: card
        radius: Theme.radius.md
        color: control.glass ? "transparent" : Theme.palette.canvas
        border.width: control.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.palette.pressedOverlay
                 : control.hovered ? Theme.palette.hoverOverlay : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
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
            // The game saves icon.png at 64x64 and shows it pixel-sharp,
            // so it is drawn the same way: at its own size, unfiltered,
            // rather than smoothed into a blur.
            Image {
                id: worldIcon
                anchors.fill: parent
                visible: control.hasIcon
                source: control.iconUrl
                fillMode: Image.PreserveAspectCrop
                smooth: false
                asynchronous: true
            }
            // On glass no one colour lies around the icon (the sky or a
            // photo shows through), so a matte would show as dark corners:
            // there the world's icon keeps the game's own square corners.
            CornerMatte {
                visible: control.hasIcon && !control.glass
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
                elide: Text.ElideRight
                color: control.nameInk
                font.family: Theme.font.family
                font.pixelSize: control.nameSize
                font.weight: Theme.type.bodyStrong.weight
            }

            // "Hardcore Survival  •  1 h ago": the instance's name gives way
            // first, so the time stays whole.
            Row {
                id: metaRow
                width: parent.width

                Text {
                    id: instanceText
                    width: Math.max(0, Math.min(implicitWidth, metaRow.width - whenText.implicitWidth))
                    text: control.instanceName
                    elide: Text.ElideRight
                    color: control.metaInk
                    font.family: Theme.font.family
                    font.pixelSize: control.metaSize
                }
                Text {
                    id: whenText
                    readonly property string when: Format.lastPlayed(control.lastPlayed)
                    text: (control.instanceName.length > 0 && when.length > 0 ? "  •  " : "") + when
                    color: control.metaInk
                    font.family: Theme.font.family
                    font.pixelSize: control.metaSize
                }
            }
        }
    }
}
