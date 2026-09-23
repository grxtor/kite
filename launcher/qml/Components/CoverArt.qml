// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Art behind one instance: its own newest screenshot when it has one, or a
 * generated plate derived from its icon tint when it does not. Used as the
 * whole cover of an InstanceCard and, full-bleed, behind ContinueCard and
 * InstancePage's hero.
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
    property int radius: Theme.radius.md + 2
    // "none" | "bottom" | "horizontal" -- a dark fade so text/controls laid
    // over a photo stay legible. Only drawn when there is a photo: the
    // fallback plate is already tuned for its own contrast. "bottom" suits
    // a card (controls sit at the cover's foot); "horizontal" suits a hero
    // (the fade favours the leading edge, where its text sits).
    property string scrim: "none"
    // Sets on the caller's own hover state -- true nudges the photo into
    // its subtle zoom.
    property bool hovered: false
    // What is painted behind this item's corners: the card or page colour.
    // Covers the photo's square corners; see the file comment.
    property color matte: Theme.palette.canvas

    readonly property bool hasPhoto: root.source.toString().length > 0
    // The zoomed photo would otherwise overshoot these bounds slightly.
    clip: true

    // The fallback art; under a photo it is only the placeholder shown
    // while the photo loads.
    Rectangle {
        id: backdrop
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Format.shade(root.tint, Theme.dark ? 0.28 : 0.88, 0.9) }
            GradientStop { position: 1.0; color: Format.shade(root.tint, Theme.dark ? 0.13 : 0.72, 0.85) }
        }

        // A few large, rotated, near-invisible squares -- a blocky hint
        // rather than a literal texture, and cheap enough for a grid of
        // these (a tiled Canvas inside each delegate would not be).
        Item {
            anchors.fill: parent
            anchors.margins: -root.width * 0.15
            visible: !root.hasPhoto
            clip: true

            Repeater {
                model: 3
                delegate: Rectangle {
                    id: block
                    required property int index
                    readonly property real span: backdrop.height * (1.05 - index * 0.24)
                    x: backdrop.width - span * 0.62 + index * span * 0.20
                    y: backdrop.height * 0.30 - span * 0.5 + index * span * 0.16
                    width: span
                    height: span
                    rotation: 18
                    radius: Theme.radius.sm
                    color: Qt.rgba(1, 1, 1, Theme.dark ? 0.045 : 0.10)
                }
            }
        }

        Image {
            anchors.centerIn: parent
            visible: !root.hasPhoto && root.iconKey.length > 0
            width: root.iconSize
            height: root.iconSize
            source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            // Pixel art: never let the scene graph filter it into a blur.
            smooth: false
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
        // resolution for what is, at most, a hero-sized crop.
        sourceSize: Qt.size(width * 2, height * 2)
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
        visible: root.hasPhoto && root.scrim === "bottom"
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.18) }
            GradientStop { position: 0.45; color: root.scrimClear }
            GradientStop { position: 1.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.62) }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.hasPhoto && root.scrim === "horizontal"
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
