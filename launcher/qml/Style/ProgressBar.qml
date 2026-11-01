// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Basic's indeterminate mode comes from a helper private to its plugin, which
// this style does not link, so indeterminate progress is a looping
// NumberAnimation on a short pill.
T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: Item {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.space.xs + Theme.space.xxs

        Rectangle {
            id: determinateFill
            visible: !control.indeterminate
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            // Never thinner than its height, so a barely started bar shows a
            // round dot rather than a sliver.
            width: control.position > 0 ? Math.max(height, parent.width * control.position) : 0
            radius: Theme.radius.pill
            color: control.enabled ? Theme.palette.accent : Theme.palette.textDisabled

            Behavior on width {
                // Data updating, not hover/press feedback, so "normal" rather
                // than "fast".
                NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }
        }

        Item {
            id: indeterminateTrack
            visible: control.indeterminate
            anchors.fill: parent
            clip: true

            Rectangle {
                id: chaser
                width: parent.width * 0.3
                height: parent.height
                radius: Theme.radius.pill
                color: Theme.palette.accent

                SequentialAnimation on x {
                    // A perpetual loading affordance, not feedback that settles,
                    // so exempt from the "nothing animates longer than
                    // Theme.motion.normal" rule.
                    loops: Animation.Infinite
                    running: control.indeterminate && control.visible
                    NumberAnimation { from: -chaser.width; to: indeterminateTrack.width; duration: 1100; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 150 }
                }
            }
        }
    }

    // The grey track Switch and Slider share.
    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.space.xs + Theme.space.xxs
        radius: Theme.radius.pill
        color: Theme.neutral.track
    }
}
