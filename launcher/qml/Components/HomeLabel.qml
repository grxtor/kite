// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A small uppercase, letter-spaced label: the hero's eyebrow ("Last
 * played"), the side panels' headings and the Sade and Gece looks' section
 * titles. One file so all of them track and weigh alike. Its colour is
 * tertiary text on the calm page and lavender on the night sky; a host over
 * a photo sets the media colours.
 */
Text {
    // The pixel size; the page's section labels and the eyebrow differ by a
    // pixel.
    property int size: Theme.type.overline.pixelSize
    // Letter spacing as a share of the size (CSS "em").
    property real tracking: 0.13

    elide: Text.ElideRight
    color: Theme.look === "night"
           ? Qt.rgba(Theme.palette.accentText.r, Theme.palette.accentText.g, Theme.palette.accentText.b, 0.65)
           : Theme.palette.textTertiary
    font.family: Theme.font.family
    font.pixelSize: size
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
    font.letterSpacing: size * tracking
}
