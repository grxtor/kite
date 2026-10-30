// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading in Home's side panel ("Kite news", "Recent worlds"):
 * quieter than the page's own headings, in the heading face at title size.
 * Sade and Gece set it as a small uppercase label like their page sections
 * (HomeLabel; lavender on the night sky); Sahne, which has no side panel,
 * uses it for the sections under "Continue playing" and sets it as
 * HomeSectionTitle does.
 */
Text {
    readonly property string look: Theme.look
    readonly property bool isLabel: look === "calm" || look === "night"
    readonly property int stageSize: 18

    elide: Text.ElideRight
    color: isLabel ? (look === "night" ? Qt.rgba(Theme.palette.accentText.r, Theme.palette.accentText.g,
                                                 Theme.palette.accentText.b, 0.65)
                                       : Theme.palette.textTertiary)
         : look === "stage" ? Theme.palette.textPrimary : Theme.palette.textSecondary
    font.family: isLabel ? Theme.font.family : Theme.font.heading
    font.pixelSize: isLabel ? Theme.type.overline.pixelSize : look === "stage" ? stageSize : Theme.type.title.pixelSize
    font.weight: isLabel ? Font.DemiBold : look === "stage" ? Font.DemiBold : Theme.type.title.weight
    font.capitalization: isLabel ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: isLabel ? Theme.type.overline.pixelSize * 0.13 : 0
}
