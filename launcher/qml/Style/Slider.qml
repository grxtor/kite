// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// An accent fill along a calm grey groove, with a white knob. Hovering the
// slider wraps the knob in a soft accent halo; dragging it widens the halo.
T.Slider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    padding: Theme.space.sm

    handle: Rectangle {
        id: knob
        x: control.leftPadding + (control.horizontal ? control.visualPosition * (control.availableWidth - width) : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : control.visualPosition * (control.availableHeight - height))
        implicitWidth: Theme.icon.md - Theme.space.xxs
        implicitHeight: Theme.icon.md - Theme.space.xxs
        radius: width / 2
        // White in both themes, like Switch's knob; a hairline keeps it
        // from melting into light mode's pale groove and white cards.
        color: Theme.media.text
        border.width: Theme.dark ? 0 : 1
        border.color: Qt.rgba(0, 0, 0, 0.12)
        opacity: control.enabled ? 1.0 : Theme.opacity.disabled

        // The hover/drag halo, under the knob.
        Rectangle {
            z: -1
            anchors.centerIn: parent
            width: parent.width + (control.pressed ? Theme.space.md : Theme.space.sm)
            height: width
            radius: width / 2
            color: Theme.palette.accentSubtle
            opacity: control.enabled && (control.hovered || control.pressed) ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
            Behavior on width {
                NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
            }
        }

        FocusRing {
            visible: control.visualFocus
        }
    }

    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        implicitWidth: control.horizontal ? Theme.control.heightLg * 4 : Theme.space.xs + Theme.space.xxs
        implicitHeight: control.horizontal ? Theme.space.xs + Theme.space.xxs : Theme.control.heightLg * 4
        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight
        radius: Theme.radius.pill
        // The same calm grey as Switch's off track: translucent, so it reads
        // on light mode's white cards as well as on dark surfaces.
        color: control.hovered && control.enabled ? Theme.neutral.trackHover : Theme.neutral.track
        scale: control.horizontal && control.mirrored ? -1 : 1

        Rectangle {
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : parent.width
            height: control.horizontal ? parent.height : control.position * parent.height
            radius: Theme.radius.pill
            color: control.enabled ? Theme.palette.accent : Theme.palette.textDisabled
        }
    }
}
