// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// ComboBox's dropdown indicator and MenuItem's submenu arrow, drawn on a
// Canvas like CheckMark so it stays crisp at any size and theme.
Canvas {
    id: chevron

    property color color: "black"
    // 0 = points down (ComboBox), 1 = points right (MenuItem submenu arrow).
    property int direction: 0

    onColorChanged: requestPaint()
    onDirectionChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = color;
        ctx.lineWidth = Math.max(1.5, width * 0.16);
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.beginPath();
        if (direction === 1) {
            ctx.moveTo(width * 0.32, height * 0.2);
            ctx.lineTo(width * 0.68, height * 0.5);
            ctx.lineTo(width * 0.32, height * 0.8);
        } else {
            ctx.moveTo(width * 0.2, height * 0.36);
            ctx.lineTo(width * 0.5, height * 0.66);
            ctx.lineTo(width * 0.8, height * 0.36);
        }
        ctx.stroke();
    }
}
