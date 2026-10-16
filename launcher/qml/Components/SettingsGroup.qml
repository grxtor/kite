// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import MeshMC.Theme

/*
 * A titled card of setting rows, separated by hairlines -- the layout every
 * settings screen of a desktop app has converged on.
 */
Column {
    id: root

    property string title
    property string description
    default property alias rows: rowsColumn.data

    spacing: Theme.space.sm

    // Title and description start on the cards' own left edge -- the same
    // x as the section heading above them -- not indented past it.
    Text {
        visible: root.title.length > 0
        text: root.title
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Font.Bold
    }

    Text {
        visible: root.description.length > 0
        width: parent.width
        text: root.description
        wrapMode: Text.Wrap
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }

    Rectangle {
        width: parent.width
        height: rowsColumn.height
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: rowsColumn
            width: parent.width

            // Hairline between rows, never above the first.
            onChildrenChanged: root.markRows()
            Component.onCompleted: root.markRows()
        }
    }

    // A row's `visible` is its effective visibility, so it reads false for
    // every row while the whole group is hidden (e.g. on a tab not shown
    // yet). Re-mark whenever the group itself or any row changes visibility,
    // instead of only when rows are added -- otherwise a group built while
    // hidden never gets its hairlines.
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
