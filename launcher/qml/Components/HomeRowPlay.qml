// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Play (Stop while the game runs) as an icon on a small square in the kite's cut, for the Sahne, Sade and Gece
 * rows and cards: quiet at rest, filled with the accent once its row is lit (hovered). Stop is the danger fill
 * whatever the row is doing, so it is never mistaken for Play.
 */
AbstractButton {
    id: control

    property bool running: false
    property bool lit: false
    property int size: Theme.control.heightSm + Theme.space.xs
    property int iconSize: Theme.icon.xs
    property color restFill: "transparent"
    property color restInk: Theme.palette.textTertiary
    // False while a host keeps the button hidden: it then grows back from appearScale as it fades in.
    property bool shown: true

    readonly property bool filled: control.running || control.lit
    readonly property color fill: control.running
        ? (down ? Qt.darker(Theme.palette.danger, 1.12) : hovered ? Qt.lighter(Theme.palette.danger, 1.08) : Theme.palette.danger)
        : control.lit ? (down ? Theme.palette.accentPressed : hovered ? Theme.palette.accentHover : Theme.palette.accent)
        : restFill
    readonly property color ink: !control.filled ? control.restInk
        : control.running && Theme.dark ? Theme.palette.canvas : Theme.palette.textOnAccent

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    padding: 0
    opacity: enabled ? 1.0 : Theme.opacity.disabled
    // Pressed, the button gives a little and comes back on release.
    scale: control.down ? Theme.motion.pressScale : 1
    Behavior on scale { NumberAnimation { duration: Theme.motion.press; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
    transform: Scale {
        origin.x: control.width / 2
        origin.y: control.height / 2
        xScale: control.shown ? 1 : Theme.motion.appearScale
        yScale: xScale
        Behavior on xScale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
    }

    // What the glyph shows, a short fade behind `running`: out, swap, in.
    property bool shownRunning: control.running
    Behavior on shownRunning {
        SequentialAnimation {
            NumberAnimation { target: glyph; property: "opacity"; to: 0; duration: Theme.motion.press }
            PropertyAction {}
            NumberAnimation { target: glyph; property: "opacity"; to: 1; duration: Theme.motion.fast }
        }
    }

    text: control.running ? qsTr("Stop") : qsTr("Play")
    Accessible.name: text

    background: KiteShape {
        radius: Theme.radius.play
        color: control.fill
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        MeshIcon {
            id: glyph
            anchors.centerIn: parent
            iconName: control.shownRunning ? "stop" : "play"
            size: control.iconSize
            color: control.ink
        }
    }
}
