// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Mods toolbar's Kite Performance button. Off, it reads "Speed up" and a
 * click turns the mode on; on, it reads "Kite Performance" with an On badge and
 * a click opens the mode's settings. The owner acts on clicked() either way.
 * Icon-only, it drops the badge and says the same in its tooltip.
 */
Button {
    id: root

    property bool on: false

    text: root.on ? qsTr("Kite Performance") : qsTr("Speed up")
    icon.source: Icons.url("zap")
    readonly property bool badged: root.on && root.display !== AbstractButton.IconOnly
    rightPadding: root.horizontalPadding + (root.badged ? badge.width + root.spacing : 0)
    ToolTip.visible: hovered && root.display === AbstractButton.IconOnly
    ToolTip.delay: 400
    ToolTip.text: root.on ? root.text + " \u00b7 " + badge.text : root.text

    StatusBadge {
        id: badge
        visible: root.badged
        anchors.right: parent.right
        anchors.rightMargin: root.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        tone: "success"
        text: qsTr("On")
    }
}
