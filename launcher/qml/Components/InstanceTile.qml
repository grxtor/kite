// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The square an instance's icon or pack logo sits in, in the kite's cut
 * (KiteShape). One file, so HomeHeroTile, HomeJumpCard, InstanceCard and
 * InstanceListRow cannot drift apart. The icon goes in as a child; a picture
 * across the whole square sets `matte` to the colour around it, and a hovered
 * host sets `lifted`. No fill and no hairline of its own.
 */
KiteShape {
    color: "transparent"
}
