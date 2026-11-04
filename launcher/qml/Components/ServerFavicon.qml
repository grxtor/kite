// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A server's favicon (the 64x64 picture its ping sends), or `glyph` in its place until there is one: "server" for a
 * server, "cube" for singleplayer and so on. `tile` puts the glyph on a sunken square, for the bigger sizes.
 */
Item {
    id: root

    // A data: URL or "".
    property string source
    property string glyph: "server"
    property int size: Theme.icon.xs
    property bool tile: false

    // What a ping sends; drawn down from it, never up.
    readonly property int sourcePixels: 64
    readonly property bool shown: picture.status === Image.Ready

    implicitWidth: root.size
    implicitHeight: root.size
    Accessible.ignored: true

    Rectangle {
        anchors.fill: parent
        visible: root.tile && !root.shown
        radius: Theme.radius.sm
        color: Theme.palette.surfaceHover
    }
    Image {
        id: picture
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.sourcePixels, root.sourcePixels)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: root.shown
    }
    MeshIcon {
        anchors.centerIn: parent
        visible: !root.shown
        iconName: root.glyph
        size: root.tile ? Math.round(root.size / 2) : root.size
        color: Theme.palette.textTertiary
    }
}
