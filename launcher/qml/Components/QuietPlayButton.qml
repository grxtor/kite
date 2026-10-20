// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Play (or Stop, while the game runs) as a secondary button, for a page
 * whose one primary Play is elsewhere -- the Home page's "Continue
 * playing" rows, under the hero's. The Style Button's secondary look with a
 * bold label, the play triangle and label turning to the accent on hover,
 * so it still reads as the thing to press. Stop keeps the danger colour on
 * its glyph and label, so the two can never be confused at a glance. Its
 * top-right corner is cut tight, like every Play (KiteShape).
 */
Button {
    id: control

    property bool running: false

    // The glyph and label's colour: accent on hover (danger for Stop),
    // the ordinary label colour at rest.
    readonly property color ink: !control.enabled ? Theme.palette.textDisabled
        : control.running ? Theme.palette.danger
        : control.hovered || control.down ? Theme.palette.accentText
        : Theme.palette.textPrimary

    variant: "secondary"
    text: control.running ? qsTr("Stop") : qsTr("Play")
    Accessible.name: control.text

    // The secondary Button's own background (Style/Button.qml) in the
    // kite's cut: the raised fill and hairline, the hover/press overlay and
    // the keyboard focus ring, which stands as far off as FocusRing's.
    background: KiteShape {
        id: plate
        implicitWidth: control.sizeHeight
        implicitHeight: control.sizeHeight
        radius: Theme.radius.md
        color: control.restColor
        borderWidth: 1
        borderColor: control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        KiteShape {
            anchors.fill: parent
            radius: plate.radius
            color: control.overlayColor
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }

        KiteShape {
            readonly property real reach: Theme.focus.width + Theme.focus.offset
            anchors.fill: parent
            anchors.margins: -reach
            radius: plate.radius + reach
            cutRadius: plate.cutRadius + reach
            borderWidth: Theme.focus.width
            borderColor: Theme.palette.focusRing
            visible: control.visualFocus
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.spacing

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: control.running ? "stop" : "play"
                size: control.sizeIcon
                color: control.ink
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                color: control.ink
                font.family: Theme.font.family
                font.pixelSize: control.sizeFont
                font.weight: Font.Bold
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }
}
