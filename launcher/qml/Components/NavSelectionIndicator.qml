// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The rail/list-nav selection: a neutral raised kite-cut shape that slides
 * between rows (a square on the icon rail, a full-width pill in a labelled
 * list) with the kite's nose lit in the accent. A host places it as a sibling
 * behind its NavItems and points `target` at the selected row; it only reads
 * that item's geometry.
 */
Item {
    id: root

    // The selected row (a NavItem or equivalent), or null to hide.
    property Item target: null

    // Icon-rail mode: a centred square instead of a full-width bar.
    property bool collapsed: false
    property int railSquare: Theme.control.height
    property int cornerRadius: Theme.radius.md
    // Where the rows' icons are centred, from a row's left edge (NavItem's iconAxis), so the square
    // neither moves nor jumps while the rail changes width; -1 centres it in the row.
    property int railAxis: -1
    // The rows are being resized under the pointer: the width follows them at once.
    property bool live: false

    // Offset from this Item's origin to where the hosted rows start (e.g. a ColumnLayout with
    // margins); hosts with no margin leave these at 0.
    property int insetX: 0
    property int insetY: 0

    // When set, the target's position is summed from its own and its parents' x/y up to this item
    // (same origin as the indicator), for hosts whose rows live in several containers.
    property Item space: null
    function _offset(item, horizontal) {
        var sum = 0
        for (var it = item; it && it !== root.space; it = it.parent)
            sum += horizontal ? it.x : it.y
        return sum
    }

    // True once the shape has a row to leave: it slides from row to row, but appears in place when the selection
    // comes back from nowhere (an instance's own tile held it on the folded rail). Set a turn late, so the first
    // placement is not animated.
    property bool placed: false
    onTargetChanged: {
        if (!root.target)
            root.placed = false
        else
            Qt.callLater(() => root.placed = !!root.target)
    }

    readonly property real targetX: !root.target ? 0 : root.space ? root._offset(root.target, true) : root.insetX
    readonly property real targetY: !root.target ? 0 : root.space ? root._offset(root.target, false) : root.insetY + root.target.y
    readonly property real targetH: root.target ? root.target.height : 0
    readonly property real targetW: root.target ? root.target.width : 0

    KiteShape {
        id: pill
        visible: !!root.target
        width: root.collapsed ? root.railSquare : root.targetW
        height: root.collapsed ? root.railSquare : root.targetH
        // Where it rests, and where it is on its way there: it slides with the pop easing and passes the row by
        // a few px at most (Theme.motion.tame), however far it came. Its size never overshoots.
        readonly property real restX: root.targetX + (!root.collapsed ? 0
                                      : root.railAxis >= 0 ? root.railAxis - root.railSquare / 2
                                      : (root.targetW - root.railSquare) / 2)
        readonly property real restY: root.targetY + (root.collapsed ? (root.targetH - height) / 2 : 0)
        property real slideX: restX
        property real slideY: restY
        property real travelX: 0
        property real travelY: 0
        onRestXChanged: travelX = restX - slideX
        onRestYChanged: travelY = restY - slideY
        x: Theme.motion.tame(slideX, restX, travelX)
        y: Theme.motion.tame(slideY, restY, travelY)
        radius: root.cornerRadius
        color: Theme.palette.surfaceRaised
        borderWidth: 1
        borderColor: Theme.palette.border

        Behavior on slideX { enabled: root.placed; NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
        Behavior on slideY { enabled: root.placed; NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot } }
        Behavior on width { enabled: root.placed && !root.live; NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
        Behavior on height { enabled: root.placed; NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    }

    KiteNose {
        visible: pill.visible
        x: pill.x + pill.width - leg
        y: pill.y
    }
}
