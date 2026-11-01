// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What goes in an InstanceTile: the instance's icon, or a platform pack's logo across the whole square once it has
 * loaded. The host reads `iconFills` and `showsPackArt` to choose the tile's matte.
 */
Item {
    id: root

    property string instanceId: ""
    property string iconKey: ""
    // A platform pack's logo; "" for none.
    property string packArt: ""
    // How much of the square a drawn icon takes.
    property real iconShare: 0.84

    readonly property string drawnKey: Icons.displayKey(root.iconKey, root.instanceId)
    readonly property bool iconFills: Icons.fillsTile(root.drawnKey)
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready

    anchors.fill: parent

    // The provider renders at the requested size, so it stays sharp at any width.
    Image {
        anchors.centerIn: parent
        visible: !root.showsPackArt
        width: root.iconFills ? parent.width : Math.round(root.width * root.iconShare)
        height: width
        source: Icons.instanceIcon(root.drawnKey)
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
    Image {
        id: packLogo
        anchors.fill: parent
        visible: root.showsPackArt
        source: root.packArt
        sourceSize: Qt.size(width * 2, height * 2)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
        asynchronous: true
    }
}
