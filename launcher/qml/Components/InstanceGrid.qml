// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The card-based instance grid. Column count is derived from width rather
 * than fixed, so the same view reads as a tight phone-width single column
 * or a wide many-column wall without the caller ever picking a number.
 */
Item {
    id: root

    property var model
    property string selectedId: ""

    signal launchRequested(string id)

    // The brief's own bounds for a card's width; kept as named properties
    // (not buried in the formula below) so the range is easy to find again.
    readonly property int minCardWidth: 180
    readonly property int maxCardWidth: 240
    readonly property int gutter: Theme.space.md
    readonly property int pagePadding: Theme.space.xl

    // Duplicated from InstanceCard's own iconExtent: GridView needs a fixed
    // cellHeight before any delegate exists to measure, so there is no way
    // to ask the card for its real size up front. Content in InstanceCard is
    // centred rather than pinned, so small drift here shows up as harmless
    // padding rather than clipped text.
    readonly property int iconExtent: Theme.icon.lg * 3
    // lineHeightPx, not lineHeight: the latter is a multiplier, and summing it
    // as pixels once reserved about four pixels for three lines of text.
    readonly property int cardHeight: Math.ceil(Theme.space.lg * 2 + root.iconExtent
            + Theme.space.sm + Theme.type.bodyStrong.lineHeightPx * 2
            + Theme.space.xxs + Theme.type.caption.lineHeightPx)

    function columnsForWidth(width) {
        var columns = Math.max(1, Math.floor((width + root.gutter) / (root.minCardWidth + root.gutter)))
        // Grows the column count further if that first guess would still
        // stretch cards past maxCardWidth (a handful of extra pixels of
        // width can otherwise land exactly between two column counts).
        while (columns > 1 && ((width + root.gutter) / columns - root.gutter) > root.maxCardWidth)
            columns++
        return columns
    }

    GridView {
        id: gridView

        /* Cells tile edge to edge and each card leaves its gutter on the
         * right, so the grid ends one gutter short of its right edge. The
         * right margin gives that gutter back: the cards then sit exactly
         * pagePadding in from both sides. */
        anchors.fill: parent
        anchors.leftMargin: root.pagePadding
        anchors.rightMargin: root.pagePadding - root.gutter
        anchors.topMargin: root.pagePadding
        bottomMargin: root.pagePadding
        clip: true
        model: root.model

        // Minus the trailing gutter the right margin handed back.
        readonly property int columns: root.columnsForWidth(width - root.gutter)

        cellWidth: width / Math.max(1, columns)
        cellHeight: root.cardHeight + root.gutter

        delegate: InstanceCard {
            // width/height inset by the gutter so GridView's own edge-to-edge
            // cell tiling reads as consistent gutters between cards.
            width: gridView.cellWidth - root.gutter
            height: gridView.cellHeight - root.gutter

            selected: root.selectedId === instanceId

            onClicked: root.selectedId = instanceId
            onDoubleClicked: root.launchRequested(instanceId)
            onPlayRequested: root.launchRequested(instanceId)
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: gridView.count === 0
        title: qsTr("No instances yet")
        body: qsTr("Create an instance to see it here.")
    }
}
