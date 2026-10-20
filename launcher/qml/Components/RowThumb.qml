// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The square a list row starts with -- a row of Content's table (PackRow)
 * or of any other instance tab (TabListRow): the thing's own picture when
 * it has one, otherwise its kind's glyph, on the neutral tile Home and the
 * library give an instance icon (PlaceholderSurface). A logo (a mod's, a
 * server's favicon) sits inside the tile, filling most of it; a picture of
 * a place (a world's icon.png) fills it edge to edge.
 */
Item {
    id: root

    // The picture; "" -- or one that has not loaded (yet) -- shows `glyph`.
    property string source: ""
    property string glyph: "package"
    // Edge to edge, cropped, instead of inset like a logo.
    property bool fill: false
    // Kept crisp, like the pixel picture it is (a world's 64 px icon).
    property bool pixelArt: false
    // Whatever the tile sits on, for the corners of a filled picture.
    property color matte: Theme.palette.surface
    readonly property bool hasPicture: picture.status === Image.Ready

    implicitWidth: Theme.control.heightLg
    implicitHeight: Theme.control.heightLg

    PlaceholderSurface {
        anchors.fill: parent
        radius: Theme.radius.md
    }
    Image {
        id: picture
        anchors.fill: parent
        anchors.margins: root.fill ? 0 : Theme.space.xs
        visible: root.hasPicture
        source: root.source
        // A logo comes from its provider at exactly this size; a filled
        // picture is cropped, so it is asked for larger.
        sourceSize: root.fill ? Qt.size(width * 2, height * 2) : Qt.size(width, height)
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
    CornerMatte {
        visible: root.fill && root.hasPicture
        cornerRadius: Theme.radius.md
        matte: root.matte
        outline: Theme.palette.border
    }
}
