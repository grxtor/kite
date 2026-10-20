// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Kite's own mark at any square size: the app icon's master cut to a square
 * centred on the kite's body (the tail left out), so all four tips stand
 * clear and the box's middle is the kite's, with the app icon's own corner
 * (22.5 %). Set `size`; the caller positions it. The picture is 256 px,
 * scaled down once to the screen's own pixels (a PNG's sourceSize is not
 * scaled by the device ratio the way an SVG's is), so it stays crisp on
 * HiDPI.
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
