// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading on the Home page ("Continue playing", "Library"). One
 * height for every heading, so every heading-to-content gap on the page
 * matches. In the classic look it is bold, in the heading face; Sahne sets
 * it semibold at 18 with the section's count (`detail`) beside it in a
 * quiet size; Sade and Gece turn it into a small uppercase, letter-spaced
 * label (lavender on the night sky) with the count after a dot.
 */
Item {
    id: root

    property string text: ""
    // The section's count: `detail` ("16 instances") beside the title in
    // Sahne; `count` (16) after a dot in Sade and Gece; neither in the
    // classic look.
    property string detail: ""
    property int count: 0

    readonly property string look: Theme.look
    readonly property bool isLabel: root.look === "calm" || root.look === "night"
    readonly property int stageSize: 18
    readonly property int detailGap: Theme.space.md

    implicitHeight: root.isLabel ? Theme.type.overline.lineHeightPx
                  : root.look === "stage" ? Theme.type.heading.lineHeightPx
                  : Theme.control.heightSm

    // The classic and Sahne title.
    Text {
        id: title
        visible: !root.isLabel
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, root.width - (counter.visible ? counter.width + root.detailGap : 0))
        text: root.text
        elide: Text.ElideRight
        color: Theme.palette.textPrimary
        font.family: Theme.font.heading
        font.pixelSize: root.look === "stage" ? root.stageSize : Theme.type.heading.pixelSize
        font.weight: root.look === "stage" ? Font.DemiBold : Font.Bold
    }
    Text {
        id: counter
        visible: root.look === "stage" && root.detail.length > 0
        x: title.width + root.detailGap
        anchors.baseline: title.baseline
        text: root.detail
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        font.weight: Font.Medium
    }

    // The Sade and Gece label.
    HomeLabel {
        visible: root.isLabel
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        text: root.count > 0 ? root.text + "  ·  " + root.count : root.text
    }
}
