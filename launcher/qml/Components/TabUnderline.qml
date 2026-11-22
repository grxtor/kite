// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The one 2 px accent underline of a row of text tabs (TabStrip, ChipGroup `tabs`): it sits on the row's hairline
 * under `target`, the selected tab, and slides and resizes to the next one over Theme.motion.slow, settling with the pop easing. A sibling of the
 * tabs' row, whose origin it shares. It appears in place (no slide from nowhere), and a tab that moves under it (a
 * resize, a count growing) takes it along at once.
 */
Rectangle {
    id: root

    // The selected tab, or null for none.
    property Item target: null

    // Trails `target` by one handler, so `sliding` is already set when x and width change.
    property Item shown: null
    property bool sliding: false
    // A turn late: while the selection moves, `target` can read null between the old tab and the new one, and
    // following that would make the line appear in place instead of sliding.
    onTargetChanged: Qt.callLater(root.follow)
    function follow() {
        if (root.shown === root.target)
            return
        root.sliding = !!root.shown && !!root.target
        root.shown = root.target
        if (root.sliding)
            settle.restart()
    }
    Component.onCompleted: root.shown = root.target

    Timer {
        id: settle
        interval: Theme.motion.slow
        onTriggered: root.sliding = false
    }

    // Where the line is on its way to the tab: the left edge slides with the pop easing, the width follows
    // without overshoot. It passes the tab by a few px at most (Theme.motion.tame) and never leaves the row: at
    // either end it is squeezed against the edge instead.
    readonly property real restX: root.shown ? root.shown.x : 0
    property real slideX: root.restX
    property real slideWidth: root.shown ? root.shown.width : 0
    property real travel: 0
    onRestXChanged: root.travel = root.restX - root.slideX
    readonly property real tamedX: Theme.motion.tame(root.slideX, root.restX, root.travel)
    readonly property real rowWidth: parent ? parent.width : 0

    x: Math.max(0, root.tamedX)
    width: Math.max(0, (root.rowWidth > 0 ? Math.min(root.rowWidth, root.tamedX + root.slideWidth)
                                          : root.tamedX + root.slideWidth) - x)
    anchors.bottom: parent.bottom
    height: 2
    color: Theme.palette.accent
    visible: !!root.shown

    Behavior on slideX {
        enabled: root.sliding
        NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
    }
    Behavior on slideWidth {
        enabled: root.sliding
        NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing }
    }
}
