// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The card a group of people on the Friends page lies in: one surface for the group (BoxSurface), its rows one
 * under the other with a hairline between them (FriendRow.divider, on every row but the first). Rows declared
 * inside, or a Repeater of them, go in the column; give each `width: card.rowWidth` and `surface: card.color`.
 */
BoxSurface {
    id: root

    default property alias rows: column.data
    readonly property int inset: Theme.space.xs
    readonly property real rowWidth: root.width - root.inset * 2

    implicitHeight: column.height + root.inset * 2

    Column {
        id: column
        x: root.inset
        y: root.inset
        width: root.rowWidth
        // The hairline's own pixel.
        spacing: 1
    }
}
