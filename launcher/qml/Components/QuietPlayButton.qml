// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Play (or Stop, while the game runs) as a secondary button, for a page
 * whose one primary Play is the play bar's (PlayDock) -- the Home page's
 * "Jump back in" rows and its header. The Style Button's secondary look,
 * with the play triangle and label turning to the accent on hover, so it
 * still reads as the thing to press. Stop keeps the danger colour on its
 * glyph and label, so the two can never be confused at a glance.
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
                font.weight: Font.DemiBold
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }
}
