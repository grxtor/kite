// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A row's or card's entrance, declared inside the delegate: it fades in and rises Theme.motion.rise px over `slow`,
 * `stagger` after the row before it (the first staggerRows rows; later ones come with the last of them).
 *
 *     delegate: PackRow {
 *         id: row
 *         RiseIn { target: row; index: row.index; gate: entrance }
 *     }
 *
 * It runs once, when the delegate is built, and only while `gate` (the view's ListEntrance) lets rows in: a row
 * built later, by scrolling or by one more item arriving, is simply there. The rise is a Translate, so the view's
 * own layout of the row is untouched; a target that already has a transform only fades. The rise settles with the
 * pop easing, the fade never overshoots. Nothing happens with motion off.
 *
 * `when: false` keeps it from running when built; replay() then runs it on demand (a settings group, each time its
 * section is opened). `gate` is anything with admit(): a ListEntrance, or a view's own rule.
 */
SequentialAnimation {
    id: root

    property Item target: null
    property int index: 0
    property var gate: null
    property bool when: true

    // Where the fade ends: the target's own opacity when it was built.
    property real rest: 1
    readonly property Translate offset: Translate {}

    PropertyAction { target: root.target; property: "opacity"; value: 0 }
    PauseAnimation {
        duration: Math.min(Math.max(root.index, 0), Theme.motion.staggerRows) * Theme.motion.stagger
    }
    ParallelAnimation {
        NumberAnimation {
            target: root.target; property: "opacity"; to: root.rest
            duration: Theme.motion.slow; easing.type: Theme.motion.easing
        }
        NumberAnimation {
            target: root.offset; property: "y"; from: Theme.motion.rise; to: 0
            duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot
        }
    }

    function replay() {
        if (!root.target || Theme.motion.reduced)
            return
        // Mid-run the target's opacity is not its own.
        if (!root.running)
            root.rest = root.target.opacity
        if (root.target.transform.length === 0)
            root.target.transform = [root.offset]
        root.restart()
    }

    Component.onCompleted: {
        if (root.when && (!root.gate || root.gate.admit()))
            root.replay()
    }
}
