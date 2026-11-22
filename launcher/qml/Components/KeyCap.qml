// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/* One key of a shortcut as a small outlined box: "Ctrl", "⌘", "F". */
Rectangle {
    id: root

    property string text: ""

    readonly property int extent: Theme.control.heightSm - Theme.space.sm

    implicitHeight: root.extent
    implicitWidth: Math.max(root.extent, Math.ceil(label.implicitWidth) + Theme.space.sm * 2)
    radius: Theme.radius.indicator
    color: "transparent"
    border.width: 1
    border.color: Theme.palette.border

    Copy {
        id: label
        anchors.centerIn: parent
        text: root.text
        role: "caption"
        tone: "secondary"
        font.weight: Font.Medium
    }
}
