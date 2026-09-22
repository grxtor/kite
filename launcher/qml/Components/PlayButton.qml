// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The launcher's one most important button. Round and icon-only on a card,
 * wide with a label in the hero; turns into Stop while the game runs, in
 * the danger colour, so the two can never be confused at a glance.
 */
AbstractButton {
    id: control

    property bool running: false
    // A launch is being prepared: the button stays in place, says so, and
    // does not fade like a disabled one.
    property bool busy: false
    property bool round: true
    property int size: Theme.control.height + 4

    text: busy ? qsTr("Starting…") : running ? qsTr("Stop") : qsTr("Play")
    hoverEnabled: true
    implicitHeight: size
    implicitWidth: round ? size : Math.max(size * 3, label.implicitWidth + Theme.icon.md + Theme.space.xl * 2 + Theme.space.sm)
    opacity: enabled || busy ? 1.0 : Theme.opacity.disabled

    Accessible.name: text

    readonly property color fill: running
        ? (down ? Qt.darker(Theme.palette.danger, 1.15) : hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
        : (down ? Theme.palette.accentPressed : hovered ? Theme.palette.accentHover : Theme.palette.accent)

    background: Rectangle {
        radius: control.round ? height / 2 : Theme.radius.md + 2
        color: control.fill
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: !control.down
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.25) }
                GradientStop { position: 0.6; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.palette.focusRing
            visible: control.visualFocus
        }
    }

    contentItem: Item {
        Row {
            anchors.centerIn: parent
            spacing: Theme.space.sm

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.busy
                iconName: control.running ? "stop" : "play"
                size: control.round ? Math.round(control.size * 0.42) : Theme.icon.md - 2
                color: Theme.palette.textOnAccent
            }

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.round
                text: control.text
                color: Theme.palette.textOnAccent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Font.Bold
            }
        }
    }
}
