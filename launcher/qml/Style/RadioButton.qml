// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Same grammar as CheckBox: a calm grey dot off, the accent with a white
// centre on.
T.RadioButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.xs
    spacing: Theme.space.sm

    indicator: Rectangle {
        id: ring
        implicitWidth: Theme.icon.md - Theme.space.xxs
        implicitHeight: Theme.icon.md - Theme.space.xxs

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: width / 2
        color: control.checked
            ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
            : (control.down || control.hovered ? Theme.neutral.trackHover : Theme.neutral.track)
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: !control.checked && control.down
            color: Theme.palette.pressedOverlay
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.4)
            height: width
            radius: width / 2
            color: Theme.palette.textOnAccent
            visible: control.checked
        }

        FocusRing {
            visible: control.visualFocus
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
