// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A small caps caption over a group of people ("PLAYING — 2"). The caller gives the words already in capitals, so a
 * language's own rules for them (Turkish dotted İ) are the translator's.
 */
Copy {
    leftPadding: Theme.space.lg
    topPadding: Theme.space.sm
    bottomPadding: Theme.space.xs
    elide: Text.ElideRight
    role: "overline"
    tone: "tertiary"
    Accessible.role: Accessible.Heading
}
