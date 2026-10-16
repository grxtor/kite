// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Art behind one instance: its own newest screenshot when it has one, or --
 * when it does not -- one of PixelArt's rendered fallback scenes, picked
 * deterministically from `seed` (an instance/world id) so a given item
 * always draws the same scene rather than a new one on every repaint.
 * Used as the whole cover of an InstanceCard and, full-bleed, behind
 * ContinueCard, InstancePage's hero and PlayDock's bar. Every scene exists
 * at three crops (see `artShape`): filling a bar far wider than 16:9 with
 * the card crop would only show a thin slice of it, so a wide host is given
 * the crop made for its own shape instead.
 *
 * The fallback scene goes through the very same Image as a screenshot --
 * same fade-in, hover zoom, decode size and scrims, no extra tint -- so an
 * instance without a screenshot looks like one that has one, only with a
 * stock scene in it.
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

    // Screenshot to show, or "" for the fallback scene.
    property url source: ""
    // The instance's own icon tint (InstanceList's iconTint role), colouring
    // the plate shown while the art decodes.
    property color tint: Theme.palette.textTertiary
    // The instance/world id driving which fallback scene is picked; falls
    // back to `iconKey` (still deterministic, just coarser-grained) when a
    // caller has no id of its own. Cards draw their own icon chip -- this
    // item is only ever the picture.
    property string iconKey: ""
    property string seed: ""
    property int radius: Theme.radius.lg
    // "none" | "bottom" | "horizontal" -- a dark fade so text/controls laid
    // over the art stay legible, whether that art is a real screenshot or
    // the fallback scene. "bottom" suits a card (controls sit
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
    // Which of PixelArt's three scene crops fits this item: "card" for a
    // grid card or tile, "band" for the instance page's hero, "strip" for the
    // dock bar.
    readonly property real aspect: root.height > 0 ? root.width / root.height : 1
    readonly property string artShape: PixelArt.shapeForAspect(root.aspect)
    readonly property string fallbackKey: root.seed.length > 0 ? root.seed : root.iconKey
    readonly property url fallbackArt: PixelArt.landscapeUrl(root.fallbackKey, root.artShape)
    // The zoomed photo would otherwise overshoot these bounds slightly.
    clip: true

    // A plate in the instance's own tint: all that shows while the art
    // decodes (or if a screenshot file turns out to be unreadable).
    Rectangle {
        id: backdrop
        anchors.fill: parent
        radius: root.radius
        color: Format.shade(root.tint, Theme.dark ? 0.20 : 0.85, 0.5)
    }

    // The screenshot, or -- without one -- one of PixelArt's rendered
    // scenes, deterministic per `seed` (see the file comment). Smooth and
    // mipmapped either way: both are photographic images decoded at up to
    // 2x their displayed size, so a 1x display draws them scaled down.
    Image {
        id: photo
        anchors.fill: parent
        visible: opacity > 0
        opacity: 0
        source: root.hasPhoto ? root.source : root.fallbackArt
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

    // Theme.media.scrim rather than the palette's scrim: the text on top is
    // light in both themes, so the fade has to be dark in both.
    readonly property color scrimOpaque: Theme.media.scrim
    readonly property color scrimClear: Qt.rgba(scrimOpaque.r, scrimOpaque.g, scrimOpaque.b, 0)

    Rectangle {
        anchors.fill: parent
        // Not gated on hasPhoto: the fallback scene wants the same
        // legibility fade a caller asks for over a real screenshot.
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
