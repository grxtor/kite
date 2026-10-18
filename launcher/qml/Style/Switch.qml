// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Accent when on, a calm grey track when off (phase-02 brief); the knob is
// white in both, with a hairline shadow so it still reads on light mode's
// pale grey track.
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
        implicitWidth: Theme.icon.lg + Theme.space.lg
        implicitHeight: Theme.icon.md + Theme.space.xxs

        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2

        radius: Theme.radius.pill
        color: control.checked
            ? (control.down ? Theme.palette.accentPressed : control.hovered ? Theme.palette.accentHover : Theme.palette.accent)
            // Translucent, so the same grey reads on canvas, a card or a
            // dialog; under the pointer it steps up like a field's border.
            : (control.hovered || control.down ? Theme.neutral.trackHover : Theme.neutral.track)
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        Behavior on color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        Rectangle {
            id: thumb
            readonly property real inset: Theme.space.xxs + 1
            width: track.height - inset * 2
            height: width
            radius: width / 2
            y: (track.height - height) / 2
            x: Math.max(inset, Math.min(track.width - width - inset,
                                        control.visualPosition * (track.width - width)))
            // Always white, in every scheme and both themes: the track says
            // on/off, the knob is just the thing you drag. A knob that also
            // changed colour (dark on green, light on grey, inverted in light
            // mode) read as a second, competing state.
            color: Theme.media.text
            border.width: Theme.dark ? 0 : 1
            border.color: Qt.rgba(0, 0, 0, 0.08)

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
