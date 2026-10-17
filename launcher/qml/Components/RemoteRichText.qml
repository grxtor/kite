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
 */
TextEdit {
    id: root

    property string source: ""
    // "markdown" or "html"
    property string format: "markdown"
    property int pixelSize: Theme.type.body.pixelSize
    readonly property font displayFont: Qt.font({ family: Theme.font.family, pixelSize: root.pixelSize })
    property bool loaded: false

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
        root.text = root.format === "html" && root.source.length > 0
                    ? "<style>a { color: " + Theme.palette.accent + "; text-decoration: none; }</style>" + root.source
                    : root.source
        root.font = Qt.binding(() => root.displayFont)
    }

    onSourceChanged: if (root.loaded) root.load()
    onFormatChanged: if (root.loaded) root.load()
    Component.onCompleted: {
        root.loaded = true
        root.load()
    }
}
