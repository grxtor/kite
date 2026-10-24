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
 */
Button {
    id: root

    // Whether Kite Performance is on for the instance.
    property bool on: false

    text: root.on ? qsTr("Kite Performance") : qsTr("Speed up")
    icon.source: Icons.url("zap")
    // Room for the badge after the label.
    rightPadding: root.horizontalPadding + (root.on ? badge.width + root.spacing : 0)

    StatusBadge {
        id: badge
        visible: root.on
        anchors.right: parent.right
        anchors.rightMargin: root.horizontalPadding
        anchors.verticalCenter: parent.verticalCenter
        tone: "success"
        text: qsTr("On")
    }
}
