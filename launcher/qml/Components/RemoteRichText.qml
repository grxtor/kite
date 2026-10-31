// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Markdown or HTML from the network (pack descriptions, changelogs, release
 * notes) shown as a read-only TextEdit that looks like Text. Text (Qt 6.11)
 * crashes the launcher when a remotely linked image fails while another is
 * still loading; TextEdit survives that.
 *
 * Set `source`, not `text`: TextEdit's `text` reads back as the re-serialised
 * document. Markdown paragraphs are spaced from the document font's point
 * size, so load() parses with the application font and only then switches to
 * the theme's pixel size.
 *
 * `article`: `source` is a whole page from RichBody (models/RichBody.h), styled
 * by the stylesheet below, which follows the theme.
 */
TextEdit {
    id: root

    property string source: ""
    property string format: "markdown"
    property bool article: false
    property int pixelSize: Theme.type.body.pixelSize
    readonly property font displayFont: Qt.font({ family: Theme.font.family, pixelSize: root.pixelSize })
    property bool loaded: false

    readonly property int h1Size: Theme.type.heading.pixelSize
    readonly property int h2Size: Theme.type.subheading.pixelSize
    readonly property int h3Size: Theme.type.title.pixelSize
    readonly property int minorHeadingSize: Theme.type.bodyStrong.pixelSize
    readonly property int lineHeightPercent: Math.round(Theme.type.body.lineHeight * 100)

    // An opaque "#rrggbb" for the stylesheet (rich text reads no alpha): translucent tokens are
    // laid over the page colour.
    function cssColor(c) {
        return Qt.tint(Theme.palette.canvas, c).toString()
    }

    readonly property string articleStyle: {
        const px = (n) => n + "px"
        const heading = (tag, size, top) => tag + " { font-size: " + px(size)
                + "; font-weight: 600; color: " + root.cssColor(Theme.palette.textPrimary)
                + "; margin-top: " + px(top) + "; margin-bottom: " + px(Theme.space.sm) + "; }"
        return "<style>"
            + "p { margin-top: 0px; margin-bottom: " + px(Theme.space.md)
            + "; line-height: " + root.lineHeightPercent + "%; }"
            + "ul, ol { margin-top: 0px; margin-bottom: " + px(Theme.space.md) + "; }"
            + "li { margin-bottom: " + px(Theme.space.xs)
            + "; line-height: " + root.lineHeightPercent + "%; }"
            + heading("h1", root.h1Size, Theme.space.xl)
            + heading("h2", root.h2Size, Theme.space.xl)
            + heading("h3", root.h3Size, Theme.space.lg)
            + heading("h4, h5, h6", root.minorHeadingSize, Theme.space.lg)
            + "b { color: " + root.cssColor(Theme.palette.textPrimary) + "; }"
            + "a { color: " + root.cssColor(Theme.palette.accentText) + "; text-decoration: none; }"
            + "code, pre { font-family: '" + Theme.font.mono + "'; background-color: "
            + root.cssColor(Theme.palette.surfaceSunken) + "; }"
            + "pre { margin-top: 0px; margin-bottom: " + px(Theme.space.md) + "; }"
            + "blockquote { margin-left: " + px(Theme.space.md) + "; color: "
            + root.cssColor(Theme.palette.textTertiary) + "; }"
            + "table.figure { margin-top: " + px(Theme.space.sm) + "; margin-bottom: " + px(Theme.space.lg) + "; }"
            // Less below a rule than above it: a heading after it brings
            // its own top margin.
            + "table.rule { margin-top: " + px(Theme.space.md) + "; margin-bottom: " + px(Theme.space.sm) + "; }"
            + "td.rule { font-size: 1px; background-color: " + root.cssColor(Theme.palette.divider) + "; }"
            + "table.grid { margin-bottom: " + px(Theme.space.md) + "; border-color: "
            + root.cssColor(Theme.palette.border) + "; }"
            + "</style>"
    }

    readOnly: true
    selectByMouse: false
    activeFocusOnPress: false
    textFormat: TextEdit.MarkdownText
    wrapMode: TextEdit.Wrap
    color: Theme.palette.textSecondary
    font: root.displayFont

    function load() {
        const appPoints = Qt.application.font.pointSize
        if (appPoints > 0)
            root.font = Qt.font({ family: Theme.font.family, pointSize: appPoints })
        root.text = ""
        root.textFormat = root.format === "html" ? TextEdit.RichText : TextEdit.MarkdownText
        // linkColor does not reach RichText; a style rule does.
        const style = root.article ? root.articleStyle
                    : "<style>a { color: " + root.cssColor(Theme.palette.accentText) + "; text-decoration: none; }</style>"
        root.text = root.format === "html" && root.source.length > 0
                    ? style + root.source
                    : root.source
        root.font = Qt.binding(() => root.displayFont)
    }

    onSourceChanged: if (root.loaded) root.load()
    onFormatChanged: if (root.loaded) root.load()
    onArticleChanged: if (root.loaded) root.load()
    // A theme switch: the stylesheet's colours are baked into the text.
    onArticleStyleChanged: if (root.loaded && root.article) root.load()
    Component.onCompleted: {
        root.loaded = true
        root.load()
    }

    HoverHandler {
        cursorShape: root.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
