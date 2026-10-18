// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Basic's indeterminate mode comes from QtQuick.Controls.Basic.impl
// (ProgressBarImpl), a helper private to that style's plugin. This style
// does not link the Basic plugin, so indeterminate progress is a plain
// looping NumberAnimation on a short pill instead -- the classic "chasing"
// indicator, needing nothing beyond QtQuick.
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
            // Never thinner than its own height, so a barely started bar
            // still shows a round dot of progress rather than a sliver.
            width: control.position > 0 ? Math.max(height, parent.width * control.position) : 0
            radius: Theme.radius.pill
            color: control.enabled ? Theme.palette.accent : Theme.palette.textDisabled

            Behavior on width {
                // A progress value is data updating, not a hover/press
                // response, so it gets the calmer "normal" duration rather
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
                    // This is a perpetual loading affordance, not a state
                    // transition, so it is exempt from the "nothing animates
                    // longer than Theme.motion.normal" rule -- that rule
                    // governs hover/press/focus/toggle feedback, which by
                    // definition settles; a "still working" indicator cannot.
                    loops: Animation.Infinite
                    running: control.indeterminate && control.visible
                    NumberAnimation { from: -chaser.width; to: indeterminateTrack.width; duration: 1100; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 150 }
                }
            }
        }
    }

    // The calm grey track Switch and Slider share.
    background: Rectangle {
        implicitWidth: Theme.control.heightLg * 4
        implicitHeight: Theme.space.xs + Theme.space.xxs
        radius: Theme.radius.pill
        color: Theme.neutral.track
    }
}
