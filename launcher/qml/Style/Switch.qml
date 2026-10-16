// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    // A bare switch (no label) is exactly its track wide, so it lines up
    // with the right edge every other control in a settings row shares;
    // with a label, the usual padding around both.
    padding: Theme.space.xs
    leftPadding: control.text ? Theme.space.xs : 0
    rightPadding: control.text ? Theme.space.xs : 0
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
            // Light mode's raised surfaces are white, like the knob; a mid
            // grey track keeps the white knob visible on a white card.
            : (Theme.dark ? Theme.palette.surfaceOverlay : Theme.palette.textDisabled)
        border.width: control.checked ? 0 : 1
        // Off has to read as a control, not as an empty slot.
        border.color: control.hovered ? Theme.palette.borderStrong : Theme.palette.border
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

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
            // Always white, in every scheme and both themes: the track says
            // on/off, the knob is just the thing you drag. A knob that also
            // changed colour (dark on green, light on grey, inverted in light
            // mode) read as a second, competing state.
            color: Theme.media.text

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
        leftPadding: control.text && control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.text && control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
        font.weight: Theme.type.body.weight
        color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
