// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Markdown or HTML from the network -- a pack's description, a changelog,
 * release notes, a plugin's text -- shown as a read-only TextEdit that looks
 * like the Text it replaces. Plain text has no images: Text is fine for it.
 *
 * Why not Text: such content links images from wherever its author put
 * them, and Text (Qt 6.11) crashes the launcher when one of them fails (a
 * 404, a refused connection, a YouTube page in an <img>) while another is
 * still loading. TextEdit loads them its own way and survives that.
 *
 * Set `source`, not `text`: TextEdit's `text` reads back as the document
 * re-serialised (never empty for HTML), so test `source` for emptiness too.
 *
 * Markdown paragraphs are spaced from the point size of the font the
 * document has when the text is parsed; a pixel-sized font has none, which
 * leaves the paragraphs touching. Text parses with the application font, so
 * load() does the same and only then switches to the theme's pixel size.
 *
 * `article`: `source` is a whole page written by RichBody (C++,
 * models/RichBody.h) -- a pack's description -- and reads like one: a type
 * scale for the headings, spaced paragraphs and lists, pictures as their
 * own centred blocks no wider than the text, quiet rules, links in the
 * accent colour. The look comes from the stylesheet below, built from the
 * theme, so it follows a theme switch.
 */
TextEdit {
    id: root

    property string source: ""
    // "markdown" or "html"
    property string format: "markdown"
    property bool article: false
    property int pixelSize: Theme.type.body.pixelSize
    readonly property font displayFont: Qt.font({ family: Theme.font.family, pixelSize: root.pixelSize })
    property bool loaded: false

    // The article's type scale, between the theme's own steps: h1 the
    // heading size, h3 the title size, h2 halfway between them.
    readonly property int h1Size: Theme.type.heading.pixelSize
    readonly property int h2Size: Math.round((Theme.type.heading.pixelSize + Theme.type.title.pixelSize) / 2)
    readonly property int h3Size: Theme.type.title.pixelSize
    readonly property int minorHeadingSize: Theme.type.bodyStrong.pixelSize
    readonly property int lineHeightPercent: Math.round(Theme.type.body.lineHeight * 100)

    // An opaque "#rrggbb" for the stylesheet (rich text reads no alpha):
    // translucent tokens are laid over the page colour first.
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
    // Set by load(), with the text, so the two always change together.
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
                    : "<style>a { color: " + Theme.palette.accent + "; text-decoration: none; }</style>"
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

    // A link is something to click: say so with the cursor.
    HoverHandler {
        cursorShape: root.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
