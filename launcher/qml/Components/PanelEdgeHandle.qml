// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A side panel's inner edge, to be dragged: a few pixels wide, with the resize
 * cursor and a line shown on hover or drag. It only reports where the pointer
 * wants the edge (`dragged`, in `space` coordinates); the panel decides the
 * width and where it snaps. A double click asks for the other mode. A press
 * that does not move is not a drag.
 */
MouseArea {
    id: root

    // The item the pointer is measured in: the one the panel is laid out in, which does not move as
    // the panel's width changes.
    property Item space: null

    // Qt.LeftEdge for a panel on the window's right, Qt.RightEdge for one on its left; decides
    // where the line is drawn and what `dragged` reports.
    property int side: Qt.LeftEdge

    readonly property int grip: Theme.space.sm - Theme.space.xxs
    readonly property int slop: Theme.space.xs

    property real _grabOffset: 0
    property real _pressedAt: 0
    property bool _dragging: false

    signal dragStarted()
    signal dragged(real edge)
    signal dragFinished()
    signal toggleRequested()

    width: root.grip
    hoverEnabled: true
    cursorShape: Qt.SizeHorCursor
    preventStealing: true
    Accessible.ignored: true

    function _edgeAt(mouse) {
        return root._pointerIn(mouse) - root._grabOffset + (root.side === Qt.RightEdge ? root.width : 0)
    }
    function _pointerIn(mouse) {
        return root.mapToItem(root.space, mouse.x, 0).x
    }

    onPressed: (mouse) => {
        root._grabOffset = mouse.x
        root._pressedAt = root._pointerIn(mouse)
        root._dragging = false
    }
    onPositionChanged: (mouse) => {
        if (!root.pressed)
            return
        var at = root._pointerIn(mouse)
        if (!root._dragging) {
            if (Math.abs(at - root._pressedAt) < root.slop)
                return
            root._dragging = true
            root.dragStarted()
        }
        root.dragged(root._edgeAt(mouse))
    }
    function _finish() {
        if (root._dragging)
            root.dragFinished()
        root._dragging = false
    }

    onReleased: root._finish()
    // The grab was taken mid-drag (e.g. the window lost focus) and no release will follow: end the
    // drag here.
    onCanceled: root._finish()
    onDoubleClicked: root.toggleRequested()

    Rectangle {
        x: root.side === Qt.LeftEdge ? 0 : parent.width - width
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.focus.width
        color: root.pressed ? Theme.palette.accent : Theme.palette.borderStrong
        opacity: root.containsMouse || root.pressed ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }
}
