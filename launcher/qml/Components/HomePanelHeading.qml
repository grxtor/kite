// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading in Home's side panel ("Kite news", "Recent worlds"): quieter than the page's headings, in the
 * heading face at title size. Sade and Gece set it as a small uppercase label (HomeLabel); Sahne uses it for the
 * sections under "Continue playing" and sets it as HomeSectionTitle does.
 */
Copy {
    readonly property string look: Theme.look
    readonly property bool isLabel: look === "calm" || look === "night"

    role: isLabel ? "overline" : look === "stage" ? "section" : "title"
    elide: Text.ElideRight
    color: isLabel ? (look === "night" ? Qt.rgba(Theme.palette.accentText.r, Theme.palette.accentText.g,
                                                 Theme.palette.accentText.b, 0.65)
                                       : Theme.palette.textTertiary)
         : look === "stage" ? Theme.palette.textPrimary : Theme.palette.textSecondary
    font.capitalization: isLabel ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: isLabel ? Theme.type.overline.pixelSize * 0.13 : 0
}
