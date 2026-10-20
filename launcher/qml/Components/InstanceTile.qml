// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The tile an instance's icon or pack logo sits on: PlaceholderSurface's
 * calm tile and hairline, in the kite's cut (KiteShape), since an instance
 * is something you can launch. One file, so HomeHeroTile, HomeJumpCard,
 * InstanceCard and InstanceListRow cannot drift apart. The icon goes in as
 * a child, centred by the host; a picture across the whole tile sets
 * `matte` to the colour around it, and a hovered host sets `lifted`.
 */
KiteShape {
    // PlaceholderSurface's step: raised in the dark theme, sunken in the
    // light one, where the cards are already white.
    color: Theme.dark ? Theme.palette.surfaceRaised : Theme.palette.surfaceSunken
    borderWidth: 1
    borderColor: Theme.palette.border
}
