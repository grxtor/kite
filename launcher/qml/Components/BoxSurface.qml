// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The launcher's bordered box: a hairline palette.border round palette.surface, or surfaceSunken (`sunken`) for
 * something that sits inset, like a list or a field's well. Never inside another box: one surface per thing.
 */
Rectangle {
    property bool sunken: false

    radius: Theme.radius.lg
    color: sunken ? Theme.palette.surfaceSunken : Theme.palette.surface
    border.width: 1
    border.color: Theme.palette.border
}
