// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The launcher's one most important button: square and icon-only on a card,
 * wide with a label in the hero; turns into Stop (danger colour) while the
 * game runs. `hero` marks the bottom play bar's instance, which adds a burst
 * of pixel squares on click. Styled like the primary Style Button, but the
 * dark theme's light coral danger fill takes the canvas colour for its label
 * (white reads at 2.6:1); the top-right corner is cut tight (KiteShape).
 */
AbstractButton {
    id: control

    property bool running: false
    property bool busy: false
    property bool round: true
    property int size: Theme.control.height + 4
    property bool hero: false

    // Bumped on every click that should burst; a one-shot Animation with no children is not a
    // reliable "now" broadcast.
    property int burstSeed: 0

    text: busy ? qsTr("Starting…") : running ? qsTr("Stop") : qsTr("Play")
    hoverEnabled: true
    implicitHeight: size
    implicitWidth: round ? size : Math.max(size * 3, label.implicitWidth + Theme.icon.md + Theme.space.xl * 2 + Theme.space.sm)
    opacity: enabled || busy ? 1.0 : Theme.opacity.disabled

    Accessible.name: text

    onClicked: if (control.hero && !control.running && !control.busy) control.burstSeed++

    readonly property color fill: running
        ? (down ? Qt.darker(Theme.palette.danger, 1.12) : hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
        : (down ? Theme.palette.accentPressed : hovered ? Theme.palette.accentHover : Theme.palette.accent)
    readonly property color ink: running && Theme.dark ? Theme.palette.canvas : Theme.palette.textOnAccent

    background: KiteShape {
        id: plate
        radius: Theme.radius.md
        color: control.fill
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        KiteShape {
            anchors.fill: parent
            radius: plate.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.10) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }

        KiteShape {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: plate.radius + Theme.space.xxs
            cutRadius: plate.cutRadius + Theme.space.xxs
            borderWidth: 2
            borderColor: Theme.palette.focusRing
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
                color: control.ink
            }

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.round
                text: control.text
                color: control.ink
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Font.Bold
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: control.hero
        clip: false

        Repeater {
            model: control.hero ? 8 : 0
            delegate: Rectangle {
                id: chip
                required property int index
                readonly property real angle: (index / 8) * Math.PI * 2
                width: 4
                height: 4
                color: index % 2 === 0 ? Theme.palette.accentHover : Theme.palette.textOnAccent
                x: control.width / 2 - width / 2
                y: control.height / 2 - height / 2
                opacity: 0

                ParallelAnimation {
                    id: fly
                    NumberAnimation { target: chip; property: "x"; to: control.width / 2 - chip.width / 2 + Math.cos(chip.angle) * control.height * 0.9; duration: 420; easing.type: Easing.OutCubic }
                    NumberAnimation { target: chip; property: "y"; to: control.height / 2 - chip.height / 2 + Math.sin(chip.angle) * control.height * 0.9; duration: 420; easing.type: Easing.OutCubic }
                    SequentialAnimation {
                        NumberAnimation { target: chip; property: "opacity"; to: 1; duration: 60 }
                        PauseAnimation { duration: 180 }
                        NumberAnimation { target: chip; property: "opacity"; to: 0; duration: 180 }
                    }
                }

                Connections {
                    target: control
                    function onBurstSeedChanged() {
                        chip.x = control.width / 2 - chip.width / 2
                        chip.y = control.height / 2 - chip.height / 2
                        fly.restart()
                    }
                }
            }
        }
    }
}
