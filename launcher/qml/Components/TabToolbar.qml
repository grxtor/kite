// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The toolbar every instance tab shares:
 *
 *     [ leading ........................ ] [⟳] [Open folder] [More ▾] [Add ...]
 *
 * Items declared inside go to the right-hand group; items for the left go in
 * `leading`. Both stay one control tall.
 */
RowLayout {
    id: root

    default property alias actions: actionRow.data
    property alias leading: leadingRow.data

    spacing: Theme.space.sm

    RowLayout {
        id: leadingRow
        Layout.fillWidth: true
        // No minimum of its own (the sum of its children's), so a search box asking for a width
        // keeps it and the row clips at the window edge. A nested layout's maximum is the sum of
        // what it holds, which would crowd the actions.
        Layout.maximumWidth: Number.POSITIVE_INFINITY
        spacing: root.spacing
    }

    RowLayout {
        id: actionRow
        spacing: root.spacing
    }
}
