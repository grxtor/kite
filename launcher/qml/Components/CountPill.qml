// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A number on the corner of an icon button (waiting requests, friends online), like NavItem's pill on a rail
 * destination. Hidden at 0. `ringColor` is the colour it sits on; `tone` is "accent" (wants an answer) or
 * "success" (who is around). As a sibling of the button's icon it needs the `z` below, or the glyph is drawn over
 * the digits.
 */
Rectangle {
    id: root

    property int count: 0
    property string tone: "accent"
    property color ringColor: Theme.palette.canvas

    readonly property int cap: 99

    z: 1
    visible: root.count > 0
    width: Math.max(height, label.implicitWidth + Theme.space.sm + Theme.focus.width * 2)
    height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.focus.width * 2
    radius: Theme.radius.pill
    border.width: Theme.focus.width
    border.color: root.ringColor
    // The dark theme's success green is light and white cannot read on it, so it takes the canvas, as NavItem's does.
    color: root.tone === "success" ? Theme.palette.success : Theme.palette.accent
    Accessible.ignored: true

    Text {
        id: label
        anchors.centerIn: parent
        text: root.count > root.cap ? root.cap + "+" : String(root.count)
        color: root.tone === "success" ? Theme.palette.canvas : Theme.palette.textOnAccent
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        font.weight: Font.DemiBold
    }
}
