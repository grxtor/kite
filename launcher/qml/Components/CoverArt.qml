// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Art behind one instance: its own newest screenshot when it has one; else,
 * for a pack installed from a platform, the pack's own logo (`packArt`, see
 * PACK ART below); else a calm accent-tinted surface (PlaceholderSurface),
 * with a Lucide glyph centred only if the host asks for one. Only real
 * pictures ever show here -- never a stand-in scene, and never an "image"
 * glyph, which reads as a broken picture. Used full-bleed behind the Home
 * page's header and ContinueCard.
 *
 * The surface is always painted underneath, so while a picture decodes -- or
 * if its file turns out to be unreadable -- the slot already looks finished.
 *
 * PACK ART. A pack's logo is square and small, so it is not stretched over
 * the cover: the cover is the logo's own colours as a soft wash (the logo
 * decoded at a few pixels and scaled back up -- no blur shader on the Qt 6.4
 * floor), and on a card-shaped cover the logo itself sits in the middle on a
 * dark glass plate (unless the host turns `showLogo` off), the way the
 * CurseForge app and Modrinth show an installed pack. A logo that cannot be
 * loaded falls back to the plain surface.
 *
 * ROUNDED CORNERS. Rectangle's `clip` only clips to the bounding box, and
 * below Qt 6.5 there is no MultiEffect/ShaderEffect-free way to mask an
 * Image to a rounded shape. So the photo fills the whole item, square, and
 * a ring drawn in `matte` -- the colour of whatever is behind this item --
 * paints over the four corners outside the rounded outline (see `corners`).
 * The caller passes the colour its own background has at that spot.
 */
Item {
    id: root

    // Screenshot to show, or "" for the plain surface.
    property url source: ""
    // A platform pack's own logo (InstanceList's packArt role), or "".
    property url packArt: ""
    // Whether the logo itself sits on a card-shaped cover over its wash; a
    // host that already shows the logo beside the name turns it off.
    property bool showLogo: true
    // A Lucide glyph centred on the plain surface, or "" (the default) for
    // none. Left out on a cover too short to hold it.
    property string glyph: ""
    property int radius: Theme.radius.lg
    // "none" | "bottom" | "horizontal" -- a dark fade so text/controls laid
    // over the art stay legible, whether that art is a real screenshot or
    // the plain surface. "bottom" suits a card (controls sit
    // at the cover's foot); "horizontal" suits a hero (the fade favours the
    // leading edge, where its text sits).
    property string scrim: "none"
    // Sets on the caller's own hover state -- true nudges the photo into
    // its subtle zoom.
    property bool hovered: false
    // 1 decodes the art near its displayed size (sharp, for a card/hero).
    // A caller compositing it as a faint background wash can pass something
    // smaller: decoding at a fraction of the size and then
    // stretching that small bitmap back up (Image's own bilinear filtering)
    // softens hard photo edges -- roofs, tree lines -- into a wash instead
    // of a blotchy low-opacity smudge, with no blur shader (Qt 6.4 floor).
    property real photoSoftness: 1.0
    // What is painted behind this item's corners: the card or page colour.
    // Covers the photo's square corners; see the file comment.
    property color matte: Theme.palette.canvas

    readonly property bool hasPhoto: root.source.toString().length > 0
    readonly property bool hasPackArt: !root.hasPhoto && root.packArt.toString().length > 0
                                       && packLogo.status !== Image.Error
    // The wash decodes the logo this many pixels across: few enough that
    // stretching it back up leaves only its colours.
    readonly property int washDecode: 6
    // The logo's plate on a card: this share of the cover's height.
    readonly property real logoShare: 0.52
    readonly property real aspect: root.height > 0 ? root.width / root.height : 1
    // A card or tile is about 16:9; the instance page's hero and the dock
    // bar are far wider, and short.
    readonly property bool cardShaped: root.aspect <= 3
    readonly property int glyphSize: 40
    readonly property int glyphMinHeight: 72
    // The zoomed photo would otherwise overshoot these bounds slightly.
    clip: true

    // The calm surface every picture sits on; all that shows without one.
    PlaceholderSurface {
        id: backdrop
        anchors.fill: parent

        MeshIcon {
            anchors.centerIn: parent
            visible: root.glyph.length > 0 && !photo.visible && !packWash.visible
                     && root.height >= root.glyphMinHeight
            iconName: root.glyph
            size: root.glyphSize
            color: Theme.palette.textTertiary
        }
    }

    // The screenshot. Smooth and mipmapped: it is decoded at up to 2x its
    // displayed size, so a 1x display draws it scaled down.
    Image {
        id: photo
        anchors.fill: parent
        visible: opacity > 0
        opacity: 0
        source: root.hasPhoto ? root.source : ""
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
        asynchronous: true
        cache: true
        // A fixed 2x cap rather than the raw item size: sharp on a retina
        // display without ever decoding a screenshot at its full
        // resolution for what is, at most, a hero-sized crop. Scaled down
        // further by photoSoftness for a caller that wants a soft wash
        // rather than a sharp photo.
        sourceSize: Qt.size(Math.max(8, width * 2 * root.photoSoftness), Math.max(8, height * 2 * root.photoSoftness))
        scale: root.hovered ? 1.04 : 1.0
        transformOrigin: Item.Center

        onStatusChanged: if (status === Image.Ready) opacity = 1

        Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
        Behavior on scale { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    }

    // -- Pack art (see PACK ART in the file comment) ------------------------
    Image {
        id: packWash
        anchors.fill: parent
        visible: root.hasPackArt && status === Image.Ready
        source: root.hasPhoto ? "" : root.packArt
        sourceSize: Qt.size(root.washDecode, root.washDecode)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
        scale: root.hovered ? 1.04 : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    }
    // Takes the wash down a step, so a white or neon logo's colours read
    // as a backdrop rather than a glare -- except under the horizontal
    // scrim, which already darkens the side the text is on; there the wash
    // keeps its colour on the other side.
    Rectangle {
        anchors.fill: parent
        visible: packWash.visible && root.scrim !== "horizontal"
        color: Theme.media.chip
    }
    Rectangle {
        id: logoPlate
        visible: root.hasPackArt && root.showLogo && root.cardShaped
        anchors.centerIn: parent
        width: Math.round(root.height * root.logoShare)
        height: width
        radius: Theme.radius.lg
        color: Theme.media.chip
        border.width: 1
        border.color: Theme.media.chipBorder

        Image {
            id: packLogo
            anchors.fill: parent
            anchors.margins: Theme.space.sm
            source: root.hasPhoto ? "" : root.packArt
            sourceSize: Qt.size(width * 2, height * 2)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            asynchronous: true
        }
    }

    // Theme.media.scrim rather than the palette's scrim: the text on top is
    // light in both themes, so the fade has to be dark in both.
    readonly property color scrimOpaque: Theme.media.scrim
    readonly property color scrimClear: Qt.rgba(scrimOpaque.r, scrimOpaque.g, scrimOpaque.b, 0)

    Rectangle {
        anchors.fill: parent
        // Not gated on hasPhoto: text laid over the plain surface wants
        // the same legibility fade a caller asks for over a screenshot.
        visible: root.scrim === "bottom"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.18) }
            GradientStop { position: 0.45; color: root.scrimClear }
            GradientStop { position: 1.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.62) }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.scrim === "horizontal"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.scrimOpaque }
            GradientStop { position: 0.45; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.62) }
            GradientStop { position: 1.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.12) }
        }
    }

    // The rounded mask: a ring whose inner edge is this item's rounded
    // outline, painted in the colour behind the item. Only its inner part
    // falls inside the item, and that is exactly the four corners.
    Rectangle {
        id: corners
        readonly property int ring: root.radius + 2
        visible: root.radius > 0
        anchors.fill: parent
        anchors.margins: -ring
        radius: root.radius + ring
        color: "transparent"
        border.width: ring
        border.color: root.matte
    }
}
