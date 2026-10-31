// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The instance's tile in the Sade and Gece looks' "Continue playing" rows, in the kite's cut with no fill of its own:
 * its icon (the built-in one while it is the generic one, Icons.displayKey) or the pack's logo. Sized by its host.
 */
KiteShape {
    id: root

    required property var row
    property color surround: Theme.palette.canvas

    readonly property string drawnKey: Icons.displayKey(root.row.iconKey, root.row.instanceId)
    readonly property bool showsPackArt: root.row.packArt.length > 0 && packLogo.status === Image.Ready
    readonly property bool iconFills: Icons.fillsTile(root.drawnKey)
    readonly property real iconShare: 0.84

    color: "transparent"
    // Over Gece's glass and sky no one colour lies behind (Theme.solidShell).
    matte: (root.showsPackArt || root.iconFills) && Theme.solidShell ? root.surround : "transparent"

    Image {
        anchors.centerIn: parent
        visible: !root.showsPackArt
        width: root.iconFills ? parent.width : Math.round(parent.width * root.iconShare)
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
        source: root.row.packArt
        sourceSize: Qt.size(width * 2, height * 2)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
        asynchronous: true
    }
}
