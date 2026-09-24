// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Art behind one instance: its own newest screenshot when it has one, or --
 * when it does not -- one of PixelArt's original pixel-art landscapes,
 * picked deterministically from `seed` (an instance/world id) so a given
 * item always draws the same scene rather than a new one on every repaint.
 * Used as the whole cover of an InstanceCard and, full-bleed, behind
 * ContinueCard, InstancePage's hero and PlayDock's bar. Every scene exists
 * at three shapes (see `artShape`): cropping the 16:9 one into a bar far
 * wider than it only ever samples a sliver of plain sky, so a wide host is
 * given the scene drawn for its own shape instead.
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

    // Screenshot to show, or "" for the generated fallback below.
    property url source: ""
    // The instance's own icon tint (InstanceList's iconTint role), driving
    // both the fallback plate and its pattern.
    property color tint: Theme.palette.textTertiary
    // image://instanceicon/<iconKey>, for the fallback's centred icon.
    property string iconKey: ""
    property int iconSize: 64
    // The instance/world id driving which fallback landscape is picked;
    // falls back to iconKey (still deterministic, just coarser-grained) when
    // a caller has not been updated to pass its own id.
    property string seed: ""
    property int radius: Theme.radius.md + 2
    // "none" | "bottom" | "horizontal" -- a dark fade so text/controls laid
    // over the art stay legible, whether that art is a real screenshot or
    // the generated pixel-art fallback. "bottom" suits a card (controls sit
    // at the cover's foot); "horizontal" suits a hero (the fade favours the
    // leading edge, where its text sits).
    property string scrim: "none"
    // Sets on the caller's own hover state -- true nudges the photo into
    // its subtle zoom.
    property bool hovered: false
    // 1 decodes the photo near its displayed size (sharp, for a card/hero).
    // A caller compositing it as a faint background wash (PageBackdrop) can
    // pass something smaller: decoding at a fraction of the size and then
    // stretching that small bitmap back up (Image's own bilinear filtering)
    // softens hard photo edges -- roofs, tree lines -- into a wash instead
    // of a blotchy low-opacity smudge, with no blur shader (Qt 6.4 floor).
    property real photoSoftness: 1.0
    // What is painted behind this item's corners: the card or page colour.
    // Covers the photo's square corners; see the file comment.
    property color matte: Theme.palette.canvas

    readonly property bool hasPhoto: root.source.toString().length > 0
    // Which of PixelArt's three drawn shapes fits this item: "card" for a
    // grid card or tile, "band" for the instance page's hero, "strip" for the
    // dock bar.
    readonly property real aspect: root.height > 0 ? root.width / root.height : 1
    readonly property string artShape: PixelArt.shapeForAspect(root.aspect)
    readonly property string fallbackKey: root.seed.length > 0 ? root.seed : root.iconKey
    readonly property url fallbackArt: PixelArt.landscapeUrl(root.fallbackKey, root.artShape)
    // The zoomed photo would otherwise overshoot these bounds slightly.
    clip: true

    // The fallback art; under a photo it is only the placeholder shown
    // while the photo loads.
    Rectangle {
        id: backdrop
        anchors.fill: parent
        radius: root.radius
        color: Format.shade(root.tint, Theme.dark ? 0.20 : 0.85, 0.5)
        clip: true

        // One of PixelArt's 24 generated scenes, deterministic per `seed`
        // (and mirrored for about half of them) -- see the file comment.
        // Nearest-neighbour (smooth: false) keeps its native pixels crisp
        // instead of letting the scene graph blur them on the upscale.
        Image {
            anchors.fill: parent
            visible: !root.hasPhoto
            source: root.hasPhoto ? "" : root.fallbackArt
            fillMode: Image.PreserveAspectCrop
            mirror: PixelArt.landscapeMirror(root.fallbackKey)
            smooth: false
            asynchronous: true
            cache: true
        }

        // Dark theme: the pastel day scenes would otherwise glare against
        // the graphite surfaces around them. Static, and only over the
        // generated art -- a real screenshot is left as it was taken.
        Rectangle {
            anchors.fill: parent
            visible: !root.hasPhoto && Theme.dark
            color: Qt.rgba(0, 0, 0, 0.22)
        }

        // A small corner badge for the instance icon rather than a giant
        // centred medallion -- same corner-anchored, icon-sized idiom
        // ModpackCard's own logoFrame uses, so the scene behind stays the
        // dominant visual and the icon reads as a badge over it.
        readonly property int badgeSize: Math.min(root.iconSize, 44)

        Rectangle {
            id: iconChip
            visible: !root.hasPhoto && root.iconKey.length > 0
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Theme.space.sm
            width: backdrop.badgeSize
            height: width
            radius: Theme.radius.md
            color: Qt.rgba(0, 0, 0, Theme.dark ? 0.34 : 0.22)

            Image {
                anchors.fill: parent
                anchors.margins: 6
                source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                // Pixel art: never let the scene graph filter it into a blur.
                smooth: false
            }
        }
    }

    Image {
        id: photo
        anchors.fill: parent
        visible: opacity > 0
        opacity: 0
        source: root.hasPhoto ? root.source : ""
        fillMode: Image.PreserveAspectCrop
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
        // Not gated on hasPhoto: the fallback landscape is real imagery too
        // and wants the same legibility fade a caller asks for over a
        // real screenshot.
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
