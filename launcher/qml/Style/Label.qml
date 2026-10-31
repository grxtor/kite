// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.Label {
    id: control

    // A default most call sites never touch.
    font.family: Theme.font.family
    font.pixelSize: Theme.type.body.pixelSize
    font.weight: Theme.type.body.weight

    color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
    // accentText, not accent: the shade the palette guarantees to read as
    // text on its surfaces.
    linkColor: Theme.palette.accentText
}
