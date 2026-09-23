// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One recently played instance in the sidebar: a click selects it, the
 * play button that appears on hover launches it straight away. In the
 * collapsed icon rail (`compact`) it shrinks to just the icon, with the
 * name as a tooltip and the play button dropped -- there is no room left
 * for it, and a click already opens the instance.
 */
AbstractButton {
    id: control

    // Filled straight from the instance model's roles when used as a
    // delegate; declared here rather than by the caller, since a caller
    // redeclaring them would shadow these and leave this file reading blanks.
    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning

    property bool compact: false

    signal playRequested()

    implicitHeight: Theme.control.height
    implicitWidth: 200
    hoverEnabled: true

    Accessible.name: name

    ToolTip.visible: control.compact && control.hovered
    ToolTip.delay: 400
    ToolTip.text: control.name

    background: Rectangle {
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        Item {
            id: icon
            anchors.left: control.compact ? undefined : parent.left
            anchors.leftMargin: Theme.space.sm
            anchors.horizontalCenter: control.compact ? parent.horizontalCenter : undefined
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.icon.lg
            height: Theme.icon.lg

            Image {
                anchors.fill: parent
                source: control.iconKey.length > 0 ? "image://instanceicon/" + control.iconKey : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
            }

            // Pulses rather than sitting static, so a glance at the rail
            // says "still running" instead of just "ran once".
            Rectangle {
                visible: control.isRunning
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: -2
                width: 8
                height: 8
                radius: 4
                color: Theme.palette.success

                SequentialAnimation on opacity {
                    running: control.isRunning
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.45; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                }
            }
        }

        Text {
            visible: !control.compact
            anchors.left: icon.right
            anchors.leftMargin: Theme.space.sm + 2
            anchors.right: trailing.left
            anchors.rightMargin: Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            text: control.name
            elide: Text.ElideRight
            color: control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.Medium
        }

        Item {
            id: trailing
            visible: !control.compact
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.xxs
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.control.heightSm
            height: width

            IconButton {
                anchors.fill: parent
                size: parent.width
                visible: control.hovered && !control.isRunning
                iconName: "play"
                tip: qsTr("Play")
                focusPolicy: Qt.NoFocus
                onClicked: control.playRequested()
            }
        }
    }
}
