// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// Basic's spinner is private to its plugin, which this style does not link
// (as in ProgressBar.qml); this draws a rounded arc on a Canvas and spins the
// item with a RotationAnimation.
T.BusyIndicator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.sm

    contentItem: Canvas {
        id: canvas
        implicitWidth: Theme.control.height
        implicitHeight: Theme.control.height

        // Canvas painting is imperative, so a theme swap needs a repaint
        // trigger; this gives onPaint's colour a dependency to react to.
        property color strokeColor: Theme.palette.accent
        onStrokeColorChanged: requestPaint()

        opacity: control.running ? 1 : 0
        Behavior on opacity {
            OpacityAnimator { duration: Theme.motion.normal }
        }

        RotationAnimation on rotation {
            running: control.running
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 900
        }

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const lineWidth = Math.max(2, width * 0.1);
            const radius = (width - lineWidth) / 2;
            ctx.lineWidth = lineWidth;
            ctx.lineCap = "round";
            ctx.strokeStyle = canvas.strokeColor;
            ctx.beginPath();
            // Three quarters of a turn reads as a ring without looking like a
            // closed, "finished" circle.
            ctx.arc(width / 2, height / 2, radius, 0, Math.PI * 1.5);
            ctx.stroke();
        }

        Component.onCompleted: requestPaint()
    }
}
