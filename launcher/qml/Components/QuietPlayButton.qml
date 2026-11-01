// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Play (or Stop, while the game runs) as a button with no frame and no fill until hovered, for a page whose one
 * primary Play is elsewhere (Home's "Continue playing" rows). The glyph and label
 * turn to the accent on hover, where the surface fills in; Stop keeps the danger colour. Its top-right
 * corner is cut tight, like every Play (KiteShape).
 */
Button {
    id: control

    property bool running: false

    readonly property color ink: !control.enabled ? Theme.palette.textDisabled
        : control.running ? Theme.palette.danger
        : control.hovered || control.down ? Theme.palette.accentText
        : Theme.palette.textPrimary

    variant: "secondary"
    text: control.running ? qsTr("Stop") : qsTr("Play")
    Accessible.name: control.text

    background: KiteShape {
        id: plate
        implicitWidth: control.sizeHeight
        implicitHeight: control.sizeHeight
        radius: Theme.radius.md
        color: (control.hovered && control.enabled) || control.down ? control.restColor : "transparent"
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
