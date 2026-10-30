// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Play (Stop while the game runs) as an icon on a small square in the
 * kite's cut, for the Sahne, Sade and Gece looks' rows and cards: quiet at
 * rest -- the host's own fill and ink -- and filled with the accent once
 * its row is lit (hovered). Stop is the danger fill whatever the row is
 * doing, so it can never be mistaken for Play. The page's one primary Play
 * is the hero's; this is its quiet sibling.
 */
AbstractButton {
    id: control

    property bool running: false
    // The row is hovered: the button takes the accent.
    property bool lit: false
    property int size: Theme.control.heightSm + Theme.space.xs
    property int iconSize: Theme.icon.sm - 2
    property color restFill: "transparent"
    property color restInk: Theme.palette.textTertiary

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

    text: control.running ? qsTr("Stop") : qsTr("Play")
    Accessible.name: text

    background: KiteShape {
        radius: Theme.radius.md - 1
        color: control.fill
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        MeshIcon {
            anchors.centerIn: parent
            iconName: control.running ? "stop" : "play"
            size: control.iconSize
            color: control.ink
        }
    }
}
