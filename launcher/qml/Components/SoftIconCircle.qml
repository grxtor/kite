// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * An icon on a soft disc: the picture of an empty state or a notice that
 * has nothing of its own to draw (Friends' "No friends yet"). `tone` is
 * "accent" (an invitation) or "warning" (something that failed).
 */
Rectangle {
    id: root

    property string iconName
    property string tone: "accent"
    property int diameter: 72
    // The glyph takes this share of the disc.
    readonly property real iconShare: 0.46

    implicitWidth: root.diameter
    implicitHeight: root.diameter
    radius: width / 2
    color: root.tone === "warning" ? Theme.palette.warningSubtle : Theme.palette.accentSubtle
    Accessible.ignored: true

    MeshIcon {
        anchors.centerIn: parent
        iconName: root.iconName
        size: Math.round(root.diameter * root.iconShare)
        color: root.tone === "warning" ? Theme.palette.warning : Theme.palette.accentText
    }
}
