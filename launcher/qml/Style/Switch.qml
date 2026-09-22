// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.xs
    spacing: Theme.space.sm

    indicator: Rectangle {
        id: track
        implicitWidth: Theme.icon.lg * 2
        implicitHeight: Theme.icon.lg

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: Theme.radius.pill
        color: control.checked
            ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
            : Theme.palette.surfaceRaised
        border.width: control.checked ? 0 : 1
        border.color: Theme.palette.border
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        Rectangle {
            id: thumb
            width: track.height - Theme.space.xxs * 2
            height: width
            radius: width / 2
            y: (track.height - height) / 2
            x: Math.max(Theme.space.xxs, Math.min(track.width - width - Theme.space.xxs,
                                                    control.visualPosition * (track.width - width)))
            color: control.checked ? Theme.palette.textOnAccent : Theme.palette.surface

            // No Behavior while the thumb is actively being dragged, so it
            // tracks the pointer 1:1; the animation is only for the
            // click-to-toggle case, where a snap would look unfinished next
            // to every other state change in this style easing in.
            Behavior on x {
                enabled: !control.down
                NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        FocusRing {
            anchors.fill: parent
            anchors.margins: -Theme.space.xxs
            radius: parent.radius
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
