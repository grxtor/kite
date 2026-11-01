// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Accent when on, a calm grey track when off; the knob is white in both, with
// a hairline shadow so it reads on light mode's pale grey track.
T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitIndicatorWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    // A bare switch is exactly its track wide, lining up with the right edge
    // other controls in a settings row share.
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
            // Translucent, so it reads on canvas, a card or a dialog.
            : (control.hovered || control.down ? Theme.neutral.trackHover : Theme.neutral.track)
        // Off, the track wears borderStrong: the translucent grey alone is
        // 1.4:1 (light) / 1.5:1 (dark) against its surface, under the 3:1 a
        // control's shape needs.
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.palette.textTertiary : Theme.palette.borderStrong
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
            // White in every scheme and both themes: the track says on/off, and
            // a knob that also changed colour read as a second, competing
            // state. Except on a near-white accent (Theme.lightAccent), where
            // a white knob would not show.
            color: control.checked && Theme.lightAccent ? Theme.palette.textOnAccent : Theme.media.text
            border.width: Theme.dark ? 0 : 1
            border.color: Theme.neutral.knobEdge

            // No animation while dragging, so the thumb tracks the pointer 1:1.
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
