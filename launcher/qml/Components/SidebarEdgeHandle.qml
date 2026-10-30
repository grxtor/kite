// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The sidebar's right edge, to be taken hold of, and what a drag of it
 * means: the same as for the friends panel on the other side of the window
 * (FriendsSidePanel). The width follows the pointer between the rail's width
 * and `maxWidth`; half way between the rail and the narrowest labelled
 * sidebar is the snap point, below which letting go folds to the rail and
 * past which it unfolds to the width reached. A double click on the edge
 * asks for the other mode.
 *
 * It reports rather than acts: `foldRequested` and `widthCommitted` are
 * emitted just before the drag ends, so SidebarNav settles on the new mode
 * in one move. While dragging, `liveWidth` is the width to show and
 * `showsRail` the mode to draw it in.
 */
PanelEdgeHandle {
    id: root

    // The widths the sidebar can have: the rail's, the narrowest labelled
    // one and the widest (never less than the narrowest).
    required property int railWidth
    required property int minWidth
    required property int maxWidth
    // The sidebar's width now, and whether it is the rail at rest.
    required property int panelWidth
    required property bool folded

    // A drag in progress, and the width the pointer asks for.
    property bool dragging: false
    property int dragWidth: 0

    // Releasing a drag below this folds the sidebar to the rail: half way
    // between the rail and the narrowest labelled sidebar.
    readonly property int snapWidth: root.railWidth + Math.round((root.minWidth - root.railWidth) / 2)
    // Whether the rail or the labelled sidebar is on show: the one at rest
    // or, while dragging, the one that letting go now would leave.
    readonly property bool showsRail: root.dragging ? root.dragWidth < root.snapWidth : root.folded
    // The width while dragging: the pointer's, except that from the snap
    // point on it is at least as wide as the labelled sidebar can be used.
    // The names would otherwise be laid out in a column narrower than they
    // are; so the edge jumps from the snap point to the minimum, and the
    // pointer catches up with it.
    readonly property int liveWidth: root.dragWidth < root.snapWidth ? root.dragWidth
                                                                     : Math.max(root.dragWidth, root.minWidth)

    // Fold to the rail (true) or unfold to the labelled sidebar (false).
    signal foldRequested(bool folded)
    // A drag ended with the sidebar labelled, at this width.
    signal widthCommitted(int width)

    side: Qt.RightEdge

    // The pointer wants the right edge at @p edge (in `space`, the sidebar's
    // parent, as PanelEdgeHandle measures it): the sidebar, whose left side
    // is its x, is as wide as that is from it, within what it may be.
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
