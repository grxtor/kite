// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * A titled card of setting rows, separated by hairlines.
 */
Column {
    id: root

    property string title
    property string description
    default property alias rows: rowsColumn.data

    spacing: Theme.space.sm

    GroupHeading {
        width: parent.width
        title: root.title
        description: root.description
    }

    BoxSurface {
        width: parent.width
        height: rowsColumn.height

        Column {
            id: rowsColumn
            width: parent.width

            // Hairline between rows, never above the first.
            onChildrenChanged: root.markRows()
            Component.onCompleted: root.markRows()
        }
    }

    // A row's `visible` is effective visibility: false for every row while the group is hidden.
    // Re-mark on any visibility change, or a group built while hidden never gets its hairlines.
    onVisibleChanged: root.markRows()
    property var _watchedRows: []

    function markRows() {
        var first = true
        for (var i = 0; i < rowsColumn.children.length; ++i) {
            var row = rowsColumn.children[i]
            if (row.showDivider === undefined)
                continue
            if (root._watchedRows.indexOf(row) < 0) {
                root._watchedRows.push(row)
                row.visibleChanged.connect(root.markRows)
            }
            row.showDivider = !first && row.visible
            if (row.visible)
                first = false
        }
    }
}
