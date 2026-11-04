// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The line under a friend's name that says where they are: "Hypixel · 1.21.4" with the server's favicon, "In Kite",
 * "Last seen 5 min ago" (Format.friendStatus() with FriendPlaces.of()). In a narrow row the version goes before the
 * text is shortened. A friend row and a conversation's header both use it. Give it a width.
 */
Row {
    id: root

    property string text
    // The game version, after a dot; "" for none.
    property string suffix
    // "primary", "secondary" or "faint".
    property string tone: "secondary"
    // The line names a server: its favicon (`icon`, a server glyph until it answers) leads it.
    property bool server: false
    property string icon

    spacing: Theme.space.xs
    visible: root.text.length > 0

    // The room the line has after the favicon; the version goes before the text is shortened.
    readonly property real textRoom: root.width - (favicon.visible ? favicon.width + root.spacing : 0)
    readonly property bool suffixFits: root.suffix.length > 0
                                       && metrics.advanceWidth + root.spacing + suffixText.implicitWidth
                                          <= root.textRoom

    // The elided text is worked out here: an elided Text keeps its full box and leaves a gap before the version.
    TextMetrics {
        id: metrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        text: root.text
        elide: Text.ElideMiddle
        elideWidth: Math.max(0, root.textRoom - (suffixText.visible ? suffixText.implicitWidth + root.spacing : 0))
    }
    ServerFavicon {
        id: favicon
        anchors.verticalCenter: parent.verticalCenter
        visible: root.server
        source: root.icon
    }
    Copy {
        text: metrics.elidedText
        role: "label"
        tone: root.tone === "faint" ? "tertiary" : root.tone
    }
    Copy {
        id: suffixText
        visible: root.suffixFits
        text: "· " + root.suffix
        role: "label"
        tone: "secondary"
    }
}
