// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The sidebar's right edge, to be dragged like the friends panel's edge
 * (FriendsSidePanel): the width follows the pointer between the rail's width
 * and `maxWidth`; releasing below the snap point folds to the rail, past it
 * unfolds to the width reached; a double click asks for the other mode. It
 * reports rather than acts: `foldRequested` and `widthCommitted` are emitted
 * just before the drag ends. While dragging, `liveWidth` is the width to show
 * and `showsRail` the mode to draw it in.
 */
PanelEdgeHandle {
    id: root

    // The widths the sidebar can have: the rail's, the narrowest labelled
    // one and the widest (never less than the narrowest).
    required property int railWidth
    required property int minWidth
    required property int maxWidth
    required property int panelWidth
    required property bool folded

    property bool dragging: false
    property int dragWidth: 0

    // Releasing below this folds to the rail: half way between the rail and the narrowest labelled
    // sidebar.
    readonly property int snapWidth: root.railWidth + Math.round((root.minWidth - root.railWidth) / 2)
    readonly property bool showsRail: root.dragging ? root.dragWidth < root.snapWidth : root.folded
    // The pointer's width, but at least the narrowest usable labelled width from the snap point on,
    // so names are never laid out in a narrower column.
    readonly property int liveWidth: root.dragWidth < root.snapWidth ? root.dragWidth
                                                                     : Math.max(root.dragWidth, root.minWidth)

    signal foldRequested(bool folded)
    signal widthCommitted(int width)

    side: Qt.RightEdge

    // The pointer wants the right edge at @p edge (in `space`, the sidebar's parent): the sidebar
    // is as wide as that is from its x, within its limits.
    function dragTo(edge) {
        var wanted = Math.round(edge - root.parent.x)
        root.dragWidth = Math.min(Math.max(wanted, root.railWidth), root.maxWidth)
    }
    function endDrag() {
        if (root.dragWidth < root.snapWidth) {
            root.foldRequested(true)
        } else {
            root.widthCommitted(Math.max(root.dragWidth, root.minWidth))
            root.foldRequested(false)
        }
        root.dragging = false
    }

    onDragStarted: {
        root.dragWidth = root.panelWidth
        root.dragging = true
    }
    onDragged: (edge) => root.dragTo(edge)
    onDragFinished: root.endDrag()
    onToggleRequested: root.foldRequested(!root.folded)
}
