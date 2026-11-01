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

    readonly property real targetX: !root.target ? 0 : root.space ? root._offset(root.target, true) : root.insetX
    readonly property real targetY: !root.target ? 0 : root.space ? root._offset(root.target, false) : root.insetY + root.target.y
    readonly property real targetH: root.target ? root.target.height : 0
    readonly property real targetW: root.target ? root.target.width : 0

    KiteShape {
        id: pill
        visible: !!root.target
        width: root.collapsed ? root.railSquare : root.targetW
        height: root.collapsed ? root.railSquare : root.targetH
        x: root.targetX + (!root.collapsed ? 0
                           : root.railAxis >= 0 ? root.railAxis - root.railSquare / 2
                           : (root.targetW - root.railSquare) / 2)
        y: root.targetY + (root.collapsed ? (root.targetH - height) / 2 : 0)
        radius: root.cornerRadius
        color: Theme.palette.surfaceRaised
        borderWidth: 1
        borderColor: Theme.palette.border

        Behavior on x { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on width { enabled: !root.live; NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on height { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    KiteNose {
        visible: pill.visible
        x: pill.x + pill.width - leg
        y: pill.y
    }
}
