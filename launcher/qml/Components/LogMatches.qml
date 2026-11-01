// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The find state of a log view: the rows that match the search text and which of them is current. Call schedule()
 * when the text or the log changes; after a pause search() asks the view to fill the rows in with setMatches().
 */
QtObject {
    id: root

    // Ascending row numbers, and the index into them being looked at (-1 when none).
    property var matches: []
    property int current: -1
    property int delayMs: 150

    signal search()
    // The user stepped to another match.
    signal stepped()

    function schedule() {
        pause.restart()
    }

    function setMatches(found) {
        root.matches = found
        root.current = found.length > 0 ? 0 : -1
    }

    function clear() {
        root.matches = []
        root.current = -1
    }

    function step(delta) {
        if (root.matches.length === 0)
            return
        root.stepped()
        root.current = (root.current + delta + root.matches.length) % root.matches.length
    }

    readonly property Timer pause: Timer {
        interval: root.delayMs
        onTriggered: root.search()
    }
}
