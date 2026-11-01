// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * macOS merges the title bar into the window (the root window's windowControlsWidth and titleBarHeight), so a
 * full-window scrim or overlay would leave nothing to grab. Put this as a child of one: it covers the top
 * titleBarHeight of its parent, moves the window on a drag, zooms it on a double click, and takes every press there
 * so none reaches the scrim's own dismiss handling. Elsewhere, and in full screen, it is not there at all.
 * Inside a popup's Overlay.modal it must be reparented above the overlay (see Kite.Style ModalScrim): the overlay
 * filters every press under it and closes the popup before a child could take it.
 */
Item {
    id: root

    // False hides the zone while its host does not (a reparented zone no longer follows it).
    property bool shown: true

    readonly property var win: Window.window
    readonly property bool active: shown && !!win && (win.windowControlsWidth ?? 0) > 0
                                   && win.visibility !== Window.FullScreen

    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: active ? win.titleBarHeight : 0
    visible: active

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons

        property point pressedAt
        property bool moving: false

        onPressed: (mouse) => {
            pressedAt = Qt.point(mouse.x, mouse.y)
            moving = false
        }
        onPositionChanged: (mouse) => {
            if (!pressed || moving || !(mouse.buttons & Qt.LeftButton))
                return
            if (Math.hypot(mouse.x - pressedAt.x, mouse.y - pressedAt.y) > Qt.styleHints.startDragDistance) {
                moving = true
                root.win.startSystemMove()
            }
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            if (root.win.visibility === Window.Maximized)
                root.win.showNormal()
            else
                root.win.showMaximized()
        }
    }
}
