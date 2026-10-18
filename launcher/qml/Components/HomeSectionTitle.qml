// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading on the Home page ("Continue playing", "Library"): bold,
 * in the heading face. One height for every heading, so every
 * heading-to-content gap on the page matches.
 */
Text {
    height: Theme.control.heightSm
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
    color: Theme.palette.textPrimary
    font.family: Theme.font.heading
    font.pixelSize: Theme.type.heading.pixelSize
    font.weight: Font.Bold
}
