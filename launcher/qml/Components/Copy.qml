// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The launcher's text: a Text that takes its size, face and colour from a type `role` and a `tone` instead of
 * five literal lines. Roles up from title use the heading face and the token weight; caption, label and body stay at
 * Font.Normal (the token weight of label is Medium, but most label text has always been regular), so set
 * font.weight when a label should be Medium. Any Text property set on the instance wins over these defaults.
 *
 * Plain text by default: much of what this shows comes from someone else (instance, world, server and mod names,
 * descriptions from mod sites), and AutoText would read tags in it as markup. A site that shows markup it made safe
 * itself sets textFormat.
 */
Text {
    id: root

    // caption, overline, label, body, bodyStrong, title, subheading, heading, display, banner, hero, heroSm, section
    // or cardName: a Theme.type entry.
    property string role: "body"
    // primary, secondary, tertiary, disabled, accent, danger, warning, success, info, onAccent, media or
    // mediaSecondary.
    property string tone: "primary"

    // The Theme.type entry of `role`, for a site that needs its lineHeight or lineHeightPx.
    readonly property QtObject spec: {
        switch (root.role) {
        case "caption": return Theme.type.caption
        case "overline": return Theme.type.overline
        case "label": return Theme.type.label
        case "bodyStrong": return Theme.type.bodyStrong
        case "title": return Theme.type.title
        case "subheading": return Theme.type.subheading
        case "heading": return Theme.type.heading
        case "display": return Theme.type.display
        case "banner": return Theme.type.banner
        case "hero": return Theme.type.hero
        case "heroSm": return Theme.type.heroSm
        case "section": return Theme.type.section
        case "cardName": return Theme.type.cardName
        default: return Theme.type.body
        }
    }
    readonly property bool headingFace: ["title", "subheading", "heading", "display", "banner", "hero", "heroSm",
                                         "section"].indexOf(root.role) >= 0
    readonly property bool regularWeight: root.role === "caption" || root.role === "label" || root.role === "body"

    color: {
        switch (root.tone) {
        case "secondary": return Theme.palette.textSecondary
        case "tertiary": return Theme.palette.textTertiary
        case "disabled": return Theme.palette.textDisabled
        case "accent": return Theme.palette.accentText
        case "danger": return Theme.palette.danger
        case "warning": return Theme.palette.warning
        case "success": return Theme.palette.success
        case "info": return Theme.palette.info
        case "onAccent": return Theme.palette.textOnAccent
        case "media": return Theme.media.text
        case "mediaSecondary": return Theme.media.textSecondary
        default: return Theme.palette.textPrimary
        }
    }
    font.family: root.headingFace ? Theme.font.heading : Theme.font.family
    font.pixelSize: root.spec.pixelSize
    font.weight: root.regularWeight ? Font.Normal : root.spec.weight
    font.letterSpacing: root.role === "overline" ? Theme.type.overline.letterSpacing : 0
    textFormat: Text.PlainText
}
