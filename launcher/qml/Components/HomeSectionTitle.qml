// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading on the Home page ("Continue playing", "Library"), one height for every heading. Classic: bold, in
 * the heading face; Sahne: semibold (section size) with the count (`detail`) beside it; Sade and Gece: a small uppercase,
 * letter-spaced label with the count after a dot.
 */
Item {
    id: root

    property string text: ""
    // The section's count: `detail` ("16 instances") in Sahne; `count` (16) after a dot in Sade and Gece.
    property string detail: ""
    property int count: 0

    readonly property string look: Theme.look
    readonly property bool isLabel: root.look === "calm" || root.look === "night"
    readonly property int detailGap: Theme.space.md

    implicitHeight: root.isLabel ? Theme.type.overline.lineHeightPx
                  : root.look === "stage" ? Theme.type.heading.lineHeightPx
                  : Theme.control.heightSm

    Copy {
        id: title
        visible: !root.isLabel
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, root.width - (counter.visible ? counter.width + root.detailGap : 0))
        text: root.text
        role: root.look === "stage" ? "section" : "heading"
        elide: Text.ElideRight
    }
    Copy {
        id: counter
        visible: root.look === "stage" && root.detail.length > 0
        x: title.width + root.detailGap
        anchors.baseline: title.baseline
        text: root.detail
        role: "caption"
        tone: "tertiary"
        font.weight: Font.Medium
    }

    HomeLabel {
        visible: root.isLabel
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        text: root.count > 0 ? root.text + "  ·  " + root.count : root.text
    }
}
