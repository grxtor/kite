// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The caption over a group of people ("In game · 2"), in sentence case like every other heading.
 */
Copy {
    leftPadding: Theme.space.xs
    topPadding: Theme.space.sm
    bottomPadding: Theme.space.sm
    elide: Text.ElideRight
    role: "label"
    tone: "secondary"
    font.weight: Font.DemiBold
    Accessible.role: Accessible.Heading
}
