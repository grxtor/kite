// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Kite's own mark at any square size: the app icon's master cut to a square on the kite's body (tail left out),
 * with the app icon's corner (22.5 %). Set `size`. The picture is scaled down once to the screen's pixels (a PNG's
 * sourceSize is not scaled by the device ratio an SVG's is), so it stays crisp on HiDPI.
 */
Image {
    property int size: 32
    readonly property int pixelExtent: Math.ceil(size * Screen.devicePixelRatio)

    width: size
    height: size
    source: "qrc:/icons/multimc/brand/kite-mark.png"
    sourceSize: Qt.size(pixelExtent, pixelExtent)
    fillMode: Image.PreserveAspectFit
}
