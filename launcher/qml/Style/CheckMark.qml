// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// The glyph for CheckBox's and MenuItem's checked state. Basic's is a PNG in
// its plugin's resources, which this style does not link, so the mark is
// drawn: Canvas strokes scale losslessly and take Theme colours directly.
Canvas {
    id: mark

    property color color: "black"
    // A short dash instead of the tick, for Qt.PartiallyChecked.
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
