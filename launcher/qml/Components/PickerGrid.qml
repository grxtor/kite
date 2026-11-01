// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The grid of a picker's tiles (LookPicker, PalettePicker): all in one row while each tile stays at least
 * `minTileWidth` wide, otherwise two by two, never three and a lone fourth. Tiles are `tileWidth` wide.
 */
Grid {
    id: root

    property int tileCount: 0
    // The narrowest tile that still reads; below it the tiles go two by two.
    property int minTileWidth: 0
    readonly property bool fitsInOneRow: (width - spacing * (tileCount - 1)) / tileCount >= minTileWidth
    readonly property real tileWidth: (width - spacing * (columns - 1)) / columns

    width: parent ? parent.width : implicitWidth
    columns: root.fitsInOneRow ? root.tileCount : 2
    spacing: Theme.space.md
}
