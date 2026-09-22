// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

// Basic's spinner comes from the same private Basic.impl helper that
// ProgressBar.qml avoids for the same reason (see its comment); this draws a
// single rounded arc on a Canvas and spins the whole item with a
// RotationAnimation, which needs nothing beyond QtQuick.
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

        // Canvas painting is imperative, not a binding, so a theme swap
        // (light/dark) needs an explicit repaint trigger -- this property
        // exists only to give onPaint's colour a dependency to react to.
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
            // Three quarters of a turn: enough to read as a ring without
            // looking like a closed, "finished" circle.
            ctx.arc(width / 2, height / 2, radius, 0, Math.PI * 1.5);
            ctx.stroke();
        }

        Component.onCompleted: requestPaint()
    }
}
