// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The instance's cover as a wide banner behind the instance page's header: the picture Home's cards show (a picked
 * cover, the newest screenshot, the pack's art or the version's title screen), cropped to fill, under a scrim that
 * keeps light text readable over a bright screenshot, fading into the page at the bottom. `shown` is false when
 * there is nothing to show: no cover, a world's 64 px icon (too blocky this wide) or the Gece look, which has no
 * covers. The header then stays plain.
 */
Item {
    id: root

    property string source: ""
    // InstanceList's coverKind.
    property string kind: ""
    readonly property bool shown: root.source.length > 0 && root.kind !== "world" && Theme.look !== "night"
    // The header's words stay above this: under it the picture is already the page.
    readonly property int fadeHeight: 40
    // A pack's logo is square and small: softened into a wash of its colours rather than blown up.
    readonly property real logoSoftness: 0.05

    readonly property color canvas: Theme.palette.canvas
    readonly property color dark: Theme.media.scrim

    visible: root.shown

    CoverArt {
        anchors.fill: parent
        source: root.shown ? root.source : ""
        kind: root.kind
        glyph: ""
        showLogo: false
        radius: 0
        photoSoftness: root.kind === "platform" ? root.logoSoftness : 1.0
    }

    // Even under the buttons, deeper on the words' side and toward the bottom line of text.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(root.dark.r, root.dark.g, root.dark.b, 0.46)
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(root.dark.r, root.dark.g, root.dark.b, 0.62) }
            GradientStop { position: 0.7; color: Qt.rgba(root.dark.r, root.dark.g, root.dark.b, 0) }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.25; color: Qt.rgba(root.dark.r, root.dark.g, root.dark.b, 0) }
            GradientStop { position: 1.0; color: Qt.rgba(root.dark.r, root.dark.g, root.dark.b, 0.6) }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.fadeHeight
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.canvas.r, root.canvas.g, root.canvas.b, 0) }
            GradientStop { position: 1.0; color: root.canvas }
        }
    }

    CornerMatte {
        matte: root.canvas
        cornerRadius: Theme.radius.lg
    }
}
