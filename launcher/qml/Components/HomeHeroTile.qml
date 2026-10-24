// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The instance's picture in Home's hero (HomeHero): its icon large on the
 * calm tile, or the pack's own logo across the whole tile once it has
 * loaded. Sized by its host. The tile is an InstanceTile, so the icon and
 * the logo alike wear the kite's cut.
 */
Item {
    id: root

    // The instanceicon provider's key, or empty for no icon.
    property string iconKey: ""
    // The pack's own logo, or empty.
    property string packArt: ""
    // What lies around the tile, for the logo's rounded corners.
    property color matte: Theme.palette.surface
    // The hero's; a smaller tile (the instance page's header) takes less.
    property int radius: Theme.radius.xl
    // Raised while a host that can be hovered is.
    property bool lifted: false

    readonly property real iconShare: 0.84
    readonly property bool iconFills: root.iconKey.length > 0 && Icons.fillsTile(root.iconKey)
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready

    InstanceTile {
        anchors.fill: parent
        radius: root.radius
        matte: root.showsPackArt || root.iconFills ? root.matte : "transparent"
        lifted: root.lifted

        Image {
            anchors.centerIn: parent
            visible: !root.showsPackArt
            width: root.iconFills ? root.width : Math.round(root.width * root.iconShare)
            height: width
            source: root.iconKey.length > 0 ? Icons.instanceIcon(root.iconKey) : ""
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
}
