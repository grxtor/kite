// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// CheckBox's checked state and MenuItem's checkable-and-checked state need
// the identical glyph. Basic's own check mark is a PNG baked into the Basic
// style plugin's resources, which this style does not link against, so the
// mark is drawn instead: a couple of Canvas strokes scale losslessly and pick
// up Theme colours directly, where a bitmap would need a re-export per size
// and per theme.
Canvas {
    id: mark

    property color color: "black"
    // Drives a short dash instead of the full tick, so CheckBox can reuse
    // this one canvas for Qt.PartiallyChecked as well as Qt.Checked instead
    // of keeping a second glyph around for one extra state.
    property bool partial: false

    onColorChanged: requestPaint()
    onPartialChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = color;
        ctx.lineWidth = Math.max(1.5, width * 0.14);
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.beginPath();
        if (partial) {
            ctx.moveTo(width * 0.22, height * 0.5);
            ctx.lineTo(width * 0.78, height * 0.5);
        } else {
            ctx.moveTo(width * 0.2, height * 0.52);
            ctx.lineTo(width * 0.42, height * 0.74);
            ctx.lineTo(width * 0.8, height * 0.28);
        }
        ctx.stroke();
    }
}
