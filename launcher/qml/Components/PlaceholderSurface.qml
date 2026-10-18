// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What a picture slot shows when there is no real picture to put in it: a
 * calm, neutral tile one step off the surface it sits on, with a hairline,
 * and -- when the host gives one -- the instance's icon or a single glyph
 * centred on it. No accent tint and no gradient: the accent is kept for
 * actions, selection and links. Built only from Theme.palette tokens, so it
 * follows whichever scheme and theme are active; nothing here is an image.
 *
 * Whatever goes in the middle is a child of this item (the default property),
 * and is centred by the host: InstanceCard and the Home rows put the
 * instance's icon there.
 */
Rectangle {
    // A raised step in the dark theme; in the light one the cards are
    // already white, so the tile steps down instead.
    color: Theme.dark ? Theme.palette.surfaceRaised : Theme.palette.surfaceSunken
    border.width: 1
    border.color: Theme.palette.border
}
