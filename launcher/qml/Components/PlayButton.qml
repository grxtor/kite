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
 *
 * `hero` marks the bottom play bar's own instance of this button: the only
 * thing it still changes is a short burst of pixel squares on click. Hover
 * and press everywhere -- hero or not -- only ever change this button's own
 * fill colour, one property, over Theme.motion.fast; an earlier pass added
 * a hover scale, a layered glow, bevel hairlines and an idle glint sweep,
 * which read as exactly the kind of "flat-and-purple, gradient-happy,
 * hovers-do-something-weird" look this app is deliberately not going for.
 */
AbstractButton {
    id: control

    property bool running: false
    // A launch is being prepared: the button stays in place, says so, and
    // does not fade like a disabled one.
    property bool busy: false
    property bool round: true
    property int size: Theme.control.height + 4
    property bool hero: false

    // Bumped on every click that should burst -- each pixel-square delegate
    // below watches this rather than a one-shot Animation's own started()
    // signal, since an Animation with no child animations to run is not a
    // reliable way to broadcast "now".
    property int burstSeed: 0

    text: busy ? qsTr("Starting…") : running ? qsTr("Stop") : qsTr("Play")
    hoverEnabled: true
    implicitHeight: size
    implicitWidth: round ? size : Math.max(size * 3, label.implicitWidth + Theme.icon.md + Theme.space.xl * 2 + Theme.space.sm)
    opacity: enabled || busy ? 1.0 : Theme.opacity.disabled

    Accessible.name: text

    onClicked: if (control.hero && !control.running && !control.busy) control.burstSeed++

    // The one property hover/press are allowed to change: a lighter fill on
    // hover, a darker one when pressed, animated over Theme.motion.fast --
    // see the file comment.
    readonly property color fill: running
        ? (down ? Qt.darker(Theme.palette.danger, 1.15) : hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
        : (down ? Theme.palette.accentPressed : hovered ? Theme.palette.accentHover : Theme.palette.accent)

    background: Rectangle {
        id: bg
        radius: control.round ? height / 2 : Theme.radius.md + 2
        color: control.fill
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // Static top gloss -- not hover-driven, just a fixed hint of light
        // from above.
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

    // A short, one-shot burst of small pixel squares in accent colours on
    // click -- purely decorative, so it runs once and stops rather than
    // looping; nothing here costs idle CPU once it finishes.
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
