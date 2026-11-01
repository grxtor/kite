// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The square a list row starts with: the thing's own picture, otherwise its
 * kind's glyph. No tile of its own. A logo fills most of the square; a picture of
 * a place (a world's icon.png) fills it edge to edge, its corners rounded in
 * `matte`.
 */
Item {
    id: root

    property string source: ""
    property string glyph: "package"
    property bool fill: false
    property bool pixelArt: false
    property color matte: Theme.palette.surface
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
    CornerMatte {
        visible: root.fill && root.hasPicture
        cornerRadius: Theme.radius.md
        matte: root.matte
    }
}
