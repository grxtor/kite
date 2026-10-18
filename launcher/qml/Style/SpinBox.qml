// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// One field (the look TextField.qml sets) with minus/plus buttons inset at
// either end, rather than Basic's two separately-coloured indicator boxes
// bolted onto a third box -- that reads as three widgets; this reads as one
// control with two active edges. Each button is a small rounded hover
// target inside the field's own corners, so its hover never squares off
// the field's rounding.
T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             up.implicitIndicatorHeight, down.implicitIndicatorHeight)

    leftPadding: control.mirrored ? (up.indicator ? up.indicator.width : 0) : (down.indicator ? down.indicator.width : 0)
    rightPadding: control.mirrored ? (down.indicator ? down.indicator.width : 0) : (up.indicator ? up.indicator.width : 0)

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        padding: Theme.space.sm

        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
        selectionColor: Theme.palette.selection
        selectedTextColor: Theme.palette.selectionText
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: SpinStep {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        plus: true
        pressed: control.up.pressed
        hovered: control.up.hovered
    }

    down.indicator: SpinStep {
        x: control.mirrored ? control.width - width : 0
        height: control.height
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        pressed: control.down.pressed
        hovered: control.down.hovered
    }

    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 3
        implicitHeight: Theme.control.height
        radius: Theme.radius.md
        color: Theme.palette.surface
        border.width: 1
        border.color: control.activeFocus && control.editable ? Theme.palette.accent
                    : control.hovered && control.enabled ? Theme.neutral.borderHover : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        FocusRing {
            halo: true
            visible: control.visualFocus || (control.editable && control.activeFocus)
        }
    }

    // One end's button: an inset rounded hover target with a minus or plus.
    // T.SpinBox itself switches an indicator's `enabled` off at the end of
    // the range, which is what greys the glyph out there.
    component SpinStep: Item {
        id: step

        property bool plus: false
        property bool pressed: false
        property bool hovered: false

        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.space.xs
            radius: Theme.radius.sm
            color: step.pressed ? Theme.palette.pressedOverlay
                 : step.hovered && step.enabled ? Theme.palette.hoverOverlay : "transparent"

            Behavior on color {
                ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Theme.icon.sm - Theme.space.xxs
            height: 2
            radius: 1
            color: step.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        }
        Rectangle {
            visible: step.plus
            anchors.centerIn: parent
            width: 2
            height: Theme.icon.sm - Theme.space.xxs
            radius: 1
            color: step.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        }
    }
}
