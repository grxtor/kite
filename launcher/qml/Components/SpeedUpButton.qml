// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Mods toolbar's Kite Performance button. While the instance's mode is
 * off it reads "Speed up" and the click turns the mode on; once it is on it
 * reads "Kite Performance" with a small On badge, and the click opens the
 * mode's settings. The owner acts on clicked() either way.
 *
 * Icon-only (a narrow toolbar) it drops the badge and says the same in its
 * tooltip.
 */
Button {
    id: root

    // Whether Kite Performance is on for the instance.
    property bool on: false

    text: root.on ? qsTr("Kite Performance") : qsTr("Speed up")
    icon.source: Icons.url("zap")
    // Room for the badge after the label.
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
