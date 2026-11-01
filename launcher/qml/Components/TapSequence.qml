// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Tells one, two and three quick taps apart. `single` fires on the first tap at once; a second tap makes it
 * `twice`, fired once the double-click interval passes with no third; a third tap fires `thrice` at once. Feed it
 * from a handler's tap with `tap()`.
 */
QtObject {
    id: root

    signal single()
    signal twice()
    signal thrice()

    property int count: 0
    property Timer settle: Timer {
        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: root.finish()
    }

    // Ends the sequence now (the timer does it when no tap follows): two taps so far count as final.
    function finish() {
        root.settle.stop()
        const taps = root.count
        root.count = 0
        if (taps === 2)
            root.twice()
    }

    function tap() {
        root.count++
        if (root.count === 1)
            root.single()
        if (root.count >= 3) {
            root.settle.stop()
            root.count = 0
            root.thrice()
            return
        }
        root.settle.restart()
    }
}
