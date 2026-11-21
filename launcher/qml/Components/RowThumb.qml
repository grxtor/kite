// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The square a list row starts with: the thing's own picture, otherwise its
 * kind's glyph. No tile of its own. A logo fills most of the square; a picture of
 * a place (a world's icon.png) fills it edge to edge, its corners rounded in
 * `matte`. A logo's corners are rounded too where the caller says so (`rounded`)
 * and gives the opaque colour behind the square as `matte`.
 */
Item {
    id: root

    property string source: ""
    property string glyph: "package"
    property bool fill: false
    property bool pixelArt: false
    property color matte: Theme.palette.surface
    property bool rounded: root.fill
    readonly property bool hasPicture: picture.status === Image.Ready

    implicitWidth: Theme.control.heightLg
    implicitHeight: Theme.control.heightLg

    Image {
        id: picture
        anchors.fill: parent
        anchors.margins: root.fill ? 0 : Theme.space.xs
        visible: root.hasPicture
        source: root.source
        // A logo comes from its provider at exactly this size; a filled picture is cropped, so it
        // is asked for larger. A pixel picture loads at its own size.
        sourceSize: root.pixelArt ? undefined
                  : root.fill ? Qt.size(width * 2, height * 2) : Qt.size(width, height)
        fillMode: root.fill ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        smooth: !root.pixelArt
        asynchronous: root.fill
    }
    MeshIcon {
        anchors.centerIn: parent
        visible: !root.hasPicture
        iconName: root.glyph
        size: Theme.icon.md
        color: Theme.palette.textTertiary
    }
    // Over what the picture really paints: a logo that is not square is letterboxed.
    Item {
        anchors.centerIn: picture
        width: Math.ceil(picture.paintedWidth)
        height: Math.ceil(picture.paintedHeight)
        visible: root.rounded && root.hasPicture

        CornerMatte {
            cornerRadius: root.fill ? Theme.radius.md : Theme.radius.sm
            matte: root.matte
        }
    }
}
