// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A side panel's inner edge, to be taken hold of: a few pixels wide, with
 * the horizontal resize cursor and a line that shows while the pointer is on
 * it or dragging. It sits at the panel's left edge or its right (`side`),
 * inside the panel. It only says where the pointer wants the edge to be
 * (`dragged`, in the coordinates of `space`); the panel (FriendsSidePanel on
 * the right of the window, SidebarNav on the left) decides what width that
 * makes and where it snaps. A double click asks for the other mode.
 *
 * A press that does not move is not a drag: a click on the edge, or the
 * first half of a double click, leaves the width alone.
 */
MouseArea {
    id: root

    // The item the pointer is measured in: the one the panel is laid out in,
    // which does not move as the panel's width changes (the handle does).
    property Item space: null

    // Which edge of the panel this stands on: Qt.LeftEdge for a panel on the
    // window's right, Qt.RightEdge for one on its left. It decides which side
    // of the handle the line is drawn on, and what `dragged` reports.
    property int side: Qt.LeftEdge

    // The few pixels that take hold; the line drawn in them is thinner.
    readonly property int grip: Theme.space.sm - Theme.space.xxs
    // How far the pointer goes before a press becomes a drag.
    readonly property int slop: Theme.space.xs

    // Where the pointer is on the handle, as a distance from the edge, when
    // the press began: the edge follows the pointer, not jumps to it.
    property real _grabOffset: 0
    property real _pressedAt: 0
    property bool _dragging: false

    signal dragStarted()
    // The panel's edge that this stands on (`side`) should be at @p edge, in
    // `space`.
    signal dragged(real edge)
    signal dragFinished()
    // Fold or unfold: the other mode.
    signal toggleRequested()

    width: root.grip
    hoverEnabled: true
    cursorShape: Qt.SizeHorCursor
    preventStealing: true
    Accessible.ignored: true

    // The handle's outer side, which is the panel's edge: its left side for a
    // panel's left edge, its right side for the right one.
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
    // The grab was taken away mid-drag (the window lost the focus, say) and
    // there will be no release to end it: the drag ends where it got to, or
    // the panel would go on following a pointer that is no longer dragging it.
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
