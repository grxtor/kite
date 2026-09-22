// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One recently played instance in the sidebar: a click selects it, the
 * play button that appears on hover launches it straight away.
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

    signal playRequested()

    implicitHeight: Theme.control.height
    implicitWidth: 200
    hoverEnabled: true

    Accessible.name: name

    background: Rectangle {
        radius: Theme.radius.md
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
    }

    contentItem: Item {
        Image {
            id: icon
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.icon.lg
            height: Theme.icon.lg
            source: control.iconKey.length > 0 ? "image://instanceicon/" + control.iconKey : ""
            sourceSize: Qt.size(width, height)
            fillMode: Image.PreserveAspectFit
        }

        Text {
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
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.xxs
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.control.heightSm
            height: width

            Rectangle {
                anchors.centerIn: parent
                width: 8
                height: 8
                radius: 4
                color: Theme.palette.success
                visible: control.isRunning && !control.hovered
            }

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
