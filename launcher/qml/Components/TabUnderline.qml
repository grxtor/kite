// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The one 2 px accent underline of a row of text tabs (TabStrip, ChipGroup `tabs`): it sits on the row's hairline
 * under `target`, the selected tab, and slides and resizes to the next one over Theme.motion.slow. A sibling of the
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
    onTargetChanged: {
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

    x: root.shown ? root.shown.x : 0
    width: root.shown ? root.shown.width : 0
    anchors.bottom: parent.bottom
    height: 2
    color: Theme.palette.accent
    visible: !!root.shown

    Behavior on x {
        enabled: root.sliding
        NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing }
    }
    Behavior on width {
        enabled: root.sliding
        NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing }
    }
}
