// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The instance's picture in Home's hero: its icon large on the calm tile, or the pack's logo across the whole tile
 * once loaded. An InstanceTile, so both wear the kite's cut.
 */
Item {
    id: root

    property string iconKey: ""
    property string instanceId: ""
    property string packArt: ""
    // What lies around the tile, for the logo's rounded corners.
    property color matte: Theme.palette.surface
    property int radius: Theme.radius.xl
    property bool lifted: false

    readonly property real iconShare: 0.84
    readonly property string drawnKey: Icons.displayKey(root.iconKey, root.instanceId)
    readonly property bool iconFills: root.drawnKey.length > 0 && Icons.fillsTile(root.drawnKey)
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
            source: root.drawnKey.length > 0 ? Icons.instanceIcon(root.drawnKey) : ""
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
