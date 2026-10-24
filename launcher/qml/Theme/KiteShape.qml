// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// The kite's cut: a rounded rectangle whose top-right corner is drawn tight
// (`cutRadius`, Theme.radius.xs), like the nose of the kite in the logo.
// Only what you can launch wears it -- an instance's tile and Play -- so
// every such shape is this one item and the cut is the same everywhere;
// search fields, menus, cards and world tiles stay plain Rectangles.
//
// The 6.4 floor has no per-corner radius (Rectangle.topRightRadius is 6.7)
// and QtQuick.Shapes only antialiases through a multisampled layer, too
// heavy for a grid of cards. So the outline is one ordinary Rectangle drawn
// three times, each copy clipped to its own share of the shape: the left
// half and the bottom-right quarter take `radius`, the top-right quarter
// `cutRadius`. The seams fall on straight runs of every copy, so fill,
// gradient and hairline meet without a step, and the copies never overlap,
// so a translucent fill (accentSubtle) stays one even tint. The clips are
// scissor rectangles and the copies the stock antialiased Rectangle.
//
// Children are drawn over the fill, like a Rectangle's. A picture that runs
// edge to edge sets `matte` to the colour around the tile, which is then
// painted over the children outside the outline, the way CornerMatte rounds
// one; it stays inside the tile's own bounds. `lifted` raises the shape by
// Theme.space.xxs -- an instance tile while its host is hovered: one
// translate over Theme.motion.fast, no shadow, no scale.
//
// Needs to be at least twice `radius` wide and tall.
Item {
    id: shape

    property real radius: Theme.radius.md
    property real cutRadius: Theme.radius.xs
    property color color: "transparent"
    // Wins over `color`, as on a Rectangle.
    property Gradient gradient: null
    property real borderWidth: 0
    property color borderColor: "transparent"
    property color matte: "transparent"
    property bool lifted: false

    transform: Translate {
        y: shape.lifted ? -Theme.space.xxs : 0
        Behavior on y { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // One copy of the outline, `grow` outside the shape (its parent's
    // size), clipped to its own share.
    component Piece: Item {
        id: piece

        property real grow
        property real corner
        property alias color: plate.color
        property alias gradient: plate.gradient
        // Plain properties, not aliases to plate.border.*: Qt before 6.11
        // rejects an alias into a grouped property nothing has bound yet
        // ("Invalid alias target location"), which made every KiteShape
        // unavailable there.
        property real borderWidth: 0
        property color borderColor: "transparent"

        clip: true

        Rectangle {
            id: plate
            x: -piece.grow - piece.x
            y: -piece.grow - piece.y
            width: piece.parent.width + piece.grow * 2
            height: piece.parent.height + piece.grow * 2
            radius: piece.corner + piece.grow
            border.width: piece.borderWidth
            border.color: piece.borderColor
        }
    }

    // The whole outline as its three pieces, clipped to the bounds of the
    // item it fills widened by `reach`.
    component Outline: Item {
        id: outline

        property real radius
        property real cutRadius
        property real grow: 0
        // Past the outer edges, so an edge's antialiasing is never cut off
        // while the shape sits between pixels (mid-lift).
        property real reach: 1
        property color color: "transparent"
        property Gradient gradient: null
        property real borderWidth: 0
        property color borderColor: "transparent"

        readonly property real splitX: Math.round(outline.width / 2)
        readonly property real splitY: Math.round(outline.height / 2)

        Piece {
            x: -outline.reach
            y: -outline.reach
            width: outline.splitX - x
            height: outline.height + outline.reach * 2
            corner: outline.radius
            grow: outline.grow
            color: outline.color
            gradient: outline.gradient
            borderWidth: outline.borderWidth
            borderColor: outline.borderColor
        }
        Piece {
            x: outline.splitX
            y: -outline.reach
            width: outline.width + outline.reach - x
            height: outline.splitY - y
            corner: outline.cutRadius
            grow: outline.grow
            color: outline.color
            gradient: outline.gradient
            borderWidth: outline.borderWidth
            borderColor: outline.borderColor
        }
        Piece {
            x: outline.splitX
            y: outline.splitY
            width: outline.width + outline.reach - x
            height: outline.height + outline.reach - y
            corner: outline.radius
            grow: outline.grow
            color: outline.color
            gradient: outline.gradient
            borderWidth: outline.borderWidth
            borderColor: outline.borderColor
        }
    }

    Outline {
        anchors.fill: parent
        visible: shape.color.a > 0 || shape.gradient !== null || shape.borderWidth > 0
        radius: shape.radius
        cutRadius: shape.cutRadius
        color: shape.color
        gradient: shape.gradient
        borderWidth: shape.borderWidth
        borderColor: shape.borderColor
    }

    // A ring as wide as the corner: its inner edge is the outline, and it
    // reaches past each corner of the bounds it is clipped to.
    Loader {
        anchors.fill: parent
        z: 1
        active: shape.matte.a > 0
        sourceComponent: Outline {
            radius: shape.radius
            cutRadius: shape.cutRadius
            grow: shape.radius
            reach: 0
            borderWidth: shape.radius
            borderColor: shape.matte
        }
    }
}
