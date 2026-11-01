// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Art behind one instance: its cover (`source`), else a platform pack's logo (`packArt`) as a soft wash, else a soft
 * gradient from the icon's colour (`tint`), else a plain PlaceholderSurface. Never a stand-in scene. Each place sets
 * how sharp a photo is (`photoSoftness`), whether it is a chosen screenshot, the pack's own art or the version's title
 * screen (`kind`).
 * Rectangle's `clip` only clips to the bounding box and Qt < 6.5 cannot mask an Image, so the photo fills the item
 * square and a ring in `matte` (the colour behind this item) paints over the four corners.
 */
Item {
    id: root

    property url source: ""
    // InstanceList's coverKind: "custom", "platform", "panorama" or "".
    property string kind: "custom"
    // The icon's colour (Icons.tint); the gradient shows when there is no photo and no pack art.
    property color tint: "transparent"
    property url packArt: ""
    // A host that already shows the logo beside the name turns it off.
    property bool showLogo: true
    property string glyph: ""
    property int radius: Theme.radius.lg
    // "none" | "bottom" | "horizontal": a dark fade for text over the art; "horizontal" favours the leading edge.
    property string scrim: "none"
    property bool hovered: false
    // 1 decodes near the displayed size. Smaller values decode less and scale up, softening photo
    // edges into a wash (no blur shader on Qt 6.4).
    property real photoSoftness: 1.0
    // The colour behind this item's corners; see the file comment.
    property color matte: Theme.palette.canvas

    readonly property bool hasPhoto: root.source.toString().length > 0
    readonly property bool panorama: root.kind === "panorama"
    readonly property bool hasTint: root.tint.valid && root.tint.a > 0
    readonly property bool showsTint: !root.hasPhoto && !root.hasPackArt && root.hasTint
    readonly property bool hasPackArt: !root.hasPhoto && root.packArt.toString().length > 0
                                       && packLogo.status !== Image.Error
    readonly property int washDecode: 6
    readonly property real logoShare: 0.52
    readonly property real aspect: root.height > 0 ? root.width / root.height : 1
    readonly property bool cardShaped: root.aspect <= 3
    readonly property int glyphMinHeight: 72
    // The panorama is 1024 px square: no point decoding past it.
    readonly property int panoramaPixels: 1024
    readonly property real tintStrongAlpha: Theme.dark ? 0.42 : 0.30
    readonly property real tintSoftAlpha: Theme.dark ? 0.12 : 0.08
    clip: true

    PlaceholderSurface {
        id: backdrop
        anchors.fill: parent
        visible: !root.showsTint
        border.width: 0

        MeshIcon {
            anchors.centerIn: parent
            visible: root.glyph.length > 0 && !photo.visible && !packWash.visible
                     && root.height >= root.glyphMinHeight
            iconName: root.glyph
            size: Theme.icon.xl
            color: Theme.palette.textTertiary
        }
    }

    // Blended onto the surface, so a dark look gets a deep colour and a light one a pastel.
    Rectangle {
        id: tintWash
        anchors.fill: parent
        visible: root.showsTint
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.tint(Theme.palette.surface, Qt.rgba(root.tint.r, root.tint.g, root.tint.b, root.tintStrongAlpha)) }
            GradientStop { position: 1.0; color: Qt.tint(Theme.palette.surface, Qt.rgba(root.tint.r, root.tint.g, root.tint.b, root.tintSoftAlpha)) }
        }

        Rectangle {
            anchors.fill: parent
            visible: root.hovered
            color: Theme.palette.hoverOverlay
        }
    }

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
        // Capped at 2x: sharp on a retina display without decoding a screenshot at full resolution. A panorama is
        // square and cropped to the item, so it decodes square at the longer side.
        sourceSize: root.panorama
                    ? Qt.size(Math.min(root.panoramaPixels, Math.max(8, Math.max(width, height) * 2 * root.photoSoftness)),
                              Math.min(root.panoramaPixels, Math.max(8, Math.max(width, height) * 2 * root.photoSoftness)))
                    : Qt.size(Math.max(8, width * 2 * root.photoSoftness), Math.max(8, height * 2 * root.photoSoftness))
        scale: root.hovered ? 1.04 : 1.0
        transformOrigin: Item.Center

        onStatusChanged: if (status === Image.Ready) opacity = 1

        Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
        Behavior on scale { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    }

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
    // Takes a white or neon logo's wash down a step; not under the horizontal scrim, which already darkens the text
    // side.
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

    // Theme.media.scrim: the text on top is light in both themes, so the fade is dark in both.
    readonly property color scrimOpaque: Theme.media.scrim
    readonly property color scrimClear: Qt.rgba(scrimOpaque.r, scrimOpaque.g, scrimOpaque.b, 0)

    Rectangle {
        anchors.fill: parent
        visible: root.scrim === "bottom"
        gradient: Gradient {
            // Held through the first line of text, which sits over the top fifth.
            GradientStop { position: 0.0; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.72) }
            GradientStop { position: 0.2; color: Qt.rgba(root.scrimOpaque.r, root.scrimOpaque.g, root.scrimOpaque.b, 0.55) }
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

    // The rounded mask: a ring whose inner edge is the item's rounded outline; only the four corners fall inside.
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
