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

    Text {
        visible: root.title.length > 0
        leftPadding: Theme.space.xs
        text: root.title
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Font.Bold
    }

    Text {
        visible: root.description.length > 0
        width: parent.width
        leftPadding: Theme.space.xs
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

    function markRows() {
        var first = true
        for (var i = 0; i < rowsColumn.children.length; ++i) {
            var row = rowsColumn.children[i]
            if (row.showDivider === undefined)
                continue
            row.showDivider = !first && row.visible
            if (row.visible)
                first = false
        }
    }
}
