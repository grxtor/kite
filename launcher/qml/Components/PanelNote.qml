// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A line or two of quiet explanation in a card or side panel: wrapped secondary
 * text at the label size. Give it a width.
 */
Text {
    wrapMode: Text.Wrap
    color: Theme.palette.textSecondary
    font.family: Theme.font.family
    font.pixelSize: Theme.type.label.pixelSize
    font.weight: Font.Normal
    lineHeight: Theme.type.label.lineHeight
}
