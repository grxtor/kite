// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The small caption above a form field ("Name", "Version") in a dialog:
 * secondary text at label size, the same as NewInstanceDialog's, so the
 * field and not its caption carries the weight.
 */
Text {
    color: Theme.palette.textSecondary
    font.family: Theme.font.family
    font.pixelSize: Theme.type.label.pixelSize
    font.weight: Font.Medium
}
