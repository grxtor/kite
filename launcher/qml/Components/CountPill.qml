// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A number on the corner of an icon button (waiting friend requests, friends
 * online): the pill NavItem draws on a rail destination, for a button that
 * is not one. Hidden at 0. Its ring is cut out of the colour it sits on
 * (`ringColor`), so it reads as laid in the corner rather than on top of the
 * glyph. `tone` says what the number is: "accent" for something that wants
 * an answer, "success" for who is around.
 *
 * It is declared inside the button, as a sibling of the button's own icon,
 * and an item stacks above its siblings that come before it only if its `z`
 * says so; hence the `z` below, without which the glyph is drawn over the
 * digits.
 */
Rectangle {
    id: root

    property int count: 0
    // "accent" or "success".
    property string tone: "accent"
    // The surface the pill's button stands on.
    property color ringColor: Theme.palette.canvas

    readonly property int cap: 99

    z: 1
    visible: root.count > 0
    width: Math.max(height, label.implicitWidth + Theme.space.sm + Theme.focus.width * 2)
    height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.focus.width * 2
    radius: Theme.radius.pill
    border.width: Theme.focus.width
    border.color: root.ringColor
    // The dark theme's success green is a light one that white cannot read
    // on, so it takes the canvas, as NavItem's does.
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
