// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The square an instance's icon or pack logo sits in, in the kite's cut
 * (KiteShape), since an instance is something you can launch. One file, so
 * HomeHeroTile, HomeJumpCard, InstanceCard and InstanceListRow cannot drift
 * apart. The icon goes in as a child, centred by the host; a picture across
 * the whole square sets `matte` to the colour around it, and a hovered host
 * sets `lifted`.
 *
 * No fill and no hairline of its own: the icon is the picture. A framed tile
 * inside a card made a box in a box, with ⋯ and Play as more boxes on top,
 * which the user turned down ("kutu içinde kutu içinde kutu", 2026-10-30).
 */
KiteShape {
    color: "transparent"
}
