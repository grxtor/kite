// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One person in the friends list, on Home's side panel and in the Friends
 * dialog: their Minecraft head, their name, and a quiet line under it (where
 * they are, or what the row is about). Whatever goes inside the row
 * (buttons, a ⋯) lands on its right.
 *
 * `presence` ("online", "playing", "offline", or "" for someone who is not
 * a friend yet) lights a small dot before the line and quiets an offline
 * friend's name. `detailStrong` sets the line in the normal text colour
 * instead of the secondary one: the server a friend is on is the point of
 * the row. A row is only a button (hover overlay, focus, clicks) when
 * `interactive` is set; in the dialog it is a plain row.
 */
AbstractButton {
    id: root

    property string uuid
    property string name
    property string detail
    property string presence: ""
    property bool detailStrong: false
    property bool interactive: false
    default property alias trailing: trailingRow.data

    // A head is pixel art: a multiple of 8, so every texel is the same size.
    readonly property int headSize: 32
    readonly property int dotSize: 6
    readonly property real offlineOpacity: 0.65
    readonly property bool lit: root.presence === "online" || root.presence === "playing"

    implicitWidth: 280
    implicitHeight: root.headSize + Theme.space.sm * 2
    hoverEnabled: root.interactive
    focusPolicy: root.interactive ? Qt.StrongFocus : Qt.NoFocus
    leftPadding: Theme.space.sm
    rightPadding: Theme.space.sm

    Accessible.name: root.detail.length > 0 ? qsTr("%1, %2").arg(root.name).arg(root.detail) : root.name

    background: Rectangle {
        radius: Theme.radius.md
        color: root.interactive && root.down ? Theme.palette.pressedOverlay
             : root.interactive && root.hovered ? Theme.palette.hoverOverlay : "transparent"
        border.width: root.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: RowLayout {
        spacing: Theme.space.md

        // A friend's head is dimmed while they are offline.
        PlayerHead {
            size: root.headSize
            opacity: root.presence === "offline" ? root.offlineOpacity : 1
            source: root.uuid.length > 0 ? "image://accountface/friend/" + root.uuid : ""
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                color: root.presence === "offline" ? Theme.palette.textSecondary : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }

            Row {
                width: parent.width
                spacing: Theme.space.xs
                visible: root.detail.length > 0

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.lit
                    width: root.dotSize
                    height: width
                    radius: width / 2
                    color: Theme.palette.success
                }
                Text {
                    width: parent.width - (root.lit ? root.dotSize + parent.spacing : 0)
                    text: root.detail
                    elide: Text.ElideRight
                    color: root.detailStrong ? Theme.palette.textPrimary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }

        Row {
            id: trailingRow
            spacing: Theme.space.xs
        }
    }
}
