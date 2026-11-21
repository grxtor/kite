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
 * own layout of the row is untouched; a target that already has a transform only fades. Nothing happens under
 * Reduce motion.
 */
SequentialAnimation {
    id: root

    property Item target: null
    property int index: 0
    property ListEntrance gate: null

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
            duration: Theme.motion.slow; easing.type: Theme.motion.easing
        }
    }

    Component.onCompleted: {
        if (!root.target || Theme.motion.reduced || (root.gate && !root.gate.admit()))
            return
        root.rest = root.target.opacity
        if (root.target.transform.length === 0)
            root.target.transform = [root.offset]
        root.start()
    }
}
