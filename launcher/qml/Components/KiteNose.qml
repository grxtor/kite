// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The kite's nose: a small accent triangle filling the cut corner of a KiteShape,
 * placed by the host at the shape's top-right (x: shape.width - leg, y: 0). One
 * file, so the nav selection and a selected instance tile light the same mark.
 */
Item {
    id: nose

    property int leg: 10

    width: leg
    height: leg
    clip: true

    Rectangle {
        width: nose.leg * Math.SQRT2
        height: width
        x: nose.leg - width / 2
        y: -height / 2
        rotation: 45
        antialiasing: true
        color: Theme.palette.accent
    }
}
