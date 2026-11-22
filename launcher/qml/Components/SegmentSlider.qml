// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The fill of a segmented toggle's active segment (ViewModeToggle, SegmentedChoice), as one piece that slides to
 * the next segment instead of jumping. A child of the group, which it fills: it is a window onto the group's own
 * rounded shape, as wide as `target` (the active segment), so the outer corners stay round, the inner edges
 * straight, and an overshoot at either end is cut by the group instead of leaving it. It appears in place.
 */
Item {
    id: root

    // The active segment, a child of a Row at the group's origin; null for none.
    property Item target: null
    property real radius: Theme.radius.sm

    // Set a turn after the first target, so the first placement is not animated.
    property bool placed: false
    onTargetChanged: {
        if (!root.target)
            root.placed = false
        else
            Qt.callLater(() => root.placed = !!root.target)
    }

    visible: !!root.target
    clip: true
    x: root.target ? root.target.x : 0
    width: root.target ? root.target.width : 0
    height: parent ? parent.height : 0

    Behavior on x {
        enabled: root.placed
        NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
    }
    Behavior on width {
        enabled: root.placed
        NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
    }

    Rectangle {
        x: -root.x
        width: root.parent ? root.parent.width : 0
        height: root.height
        radius: root.radius
        color: Theme.palette.hoverOverlay
    }
}
