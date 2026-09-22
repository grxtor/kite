// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

// One pill-shaped field with plus/minus zones at either end, rather than
// Basic's two separately-coloured indicator boxes bolted onto a third box --
// that reads as three widgets; this reads as one control with two active
// edges, closer to the calmer, single-surface language the rest of the style
// uses for fields.
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

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : control.width - width
        height: control.height
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        color: control.up.pressed ? Theme.palette.pressedOverlay : control.up.hovered ? Theme.palette.hoverOverlay : "transparent"

        Rectangle {
            x: 0
            width: 1
            height: parent.height
            color: Theme.palette.divider
        }

        Rectangle {
            anchors.centerIn: parent
            width: Theme.icon.sm
            height: 2
            radius: 1
            color: control.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        }
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: Theme.icon.sm
            radius: 1
            color: control.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? control.width - width : 0
        height: control.height
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height
        color: control.down.pressed ? Theme.palette.pressedOverlay : control.down.hovered ? Theme.palette.hoverOverlay : "transparent"

        Rectangle {
            x: parent.width - 1
            width: 1
            height: parent.height
            color: Theme.palette.divider
        }

        Rectangle {
            anchors.centerIn: parent
            width: Theme.icon.sm
            height: 2
            radius: 1
            color: control.enabled ? Theme.palette.textSecondary : Theme.palette.textDisabled
        }
    }

    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 3
        radius: Theme.radius.md
        color: Theme.palette.surfaceSunken
        border.width: 1
        border.color: control.activeFocus ? Theme.palette.borderStrong : Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45
        clip: true

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius + Theme.space.xxs
            visible: control.visualFocus
        }
    }
}
