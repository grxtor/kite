// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The dot that says an instance is up: green while it runs, amber while its launch is still starting the game. With
 * `pulsing` it fades in and out; that stops with the window, an off-screen page and "Reduce motion".
 */
Rectangle {
    id: root

    property bool launching: false
    property int size: 9
    property bool pulsing: false

    width: root.size
    height: root.size
    radius: root.size / 2
    color: root.launching ? Theme.palette.warning : Theme.palette.success

    SequentialAnimation on opacity {
        running: root.pulsing && root.visible && Qt.application.state === Qt.ApplicationActive && !Theme.motion.reduced
        loops: Animation.Infinite
        NumberAnimation { from: 1.0; to: 0.45; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
        NumberAnimation { from: 0.45; to: 1.0; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
    }
}
