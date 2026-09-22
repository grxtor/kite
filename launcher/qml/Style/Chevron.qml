// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// Shared by ComboBox (dropdown indicator) and MenuItem (submenu arrow), which
// otherwise wanted the same "v" glyph in two different rotations. Drawn on a
// Canvas for the same reason as CheckMark: no bitmap asset to re-export per
// theme/size, and it stays crisp at any of Theme.icon's sizes.
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
