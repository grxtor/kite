// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The rail/list-nav selection grammar (design-plan.md Principle 6): a
 * surfaceRaised pill that slides between rows, plus a 3px accent bar pinned
 * to the pill's leading edge -- both siblings of whatever draws the rows
 * themselves (a NavItem only ever paints its own transient hover/press
 * state; the pill is what actually shows "this one is selected").
 *
 * Extracted out of SidebarNav.qml so every rail/list-nav host shares one
 * mechanism instead of each re-deriving the same geometry: SidebarNav uses
 * it for the main rail, and SettingsPage's section list uses it too, rather
 * than falling back to NavItem's own bare accent-icon/bold-text look with no
 * pill at all.
 *
 * A host places this as a sibling *behind* its row of NavItems (declared
 * first, so it paints first) and points `target` at the currently selected
 * row's Item -- this component only reads that item's y/height/width, it
 * never owns the rows or their model.
 */
Item {
    id: root

    // The selected row (a NavItem or equivalent), or null to hide entirely
    // -- e.g. a page the host's own list doesn't contain.
    property Item target: null

    // Icon-rail mode: a centred square matching the row's own collapsed
    // square, rather than a bar stretched to the row's full width. Hosts
    // that never collapse (Settings' section list) leave this false.
    property bool collapsed: false
    property int railSquare: 40
    // Where the rows' icons are centred, from a row's left edge (NavItem's
    // iconAxis): the collapsed square sits on that axis, not on the row's
    // middle, so it neither moves nor jumps while the rail changes width.
    // -1 centres it in the row instead.
    property int railAxis: -1

    // Offset from this Item's own origin to where the hosted rows actually
    // start -- e.g. SidebarNav's rows sit inside a ColumnLayout with
    // `anchors.margins: Theme.space.md`, so the pill needs the same offset
    // to land on them; a host with no such margin leaves these at 0.
    property int insetX: 0
    property int insetY: 0
    // How far the accent bar stops short of the row's top and bottom.
    property int barInset: root.insetY

    // When set, the target's position is found by adding up its own and its
    // parents' x/y up to this item (which must share this indicator's
    // origin) -- for a host whose rows live in more than one container, e.g.
    // SidebarNav's foot band below its main column. The insets are then not
    // needed. Bindings re-evaluate when any of those x/y values change,
    // since the sum reads each of them.
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

    Rectangle {
        id: pill
        visible: !!root.target
        width: root.collapsed ? root.railSquare : root.targetW
        height: root.collapsed ? root.railSquare : root.targetH
        x: root.targetX + (!root.collapsed ? 0
                           : root.railAxis >= 0 ? root.railAxis - root.railSquare / 2
                           : (root.targetW - root.railSquare) / 2)
        y: root.targetY + (root.collapsed ? (root.targetH - height) / 2 : 0)
        radius: Theme.radius.md
        color: Theme.palette.surfaceRaised

        Behavior on x { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on width { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    // On the pill's leading edge in both modes, as in the expanded rail.
    Rectangle {
        visible: !!root.target
        x: pill.x
        y: root.targetY + (root.targetH - height) / 2
        width: 3
        height: root.targetH - root.barInset * 2 + 4
        radius: Theme.radius.xs
        color: Theme.palette.accent

        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }
}
