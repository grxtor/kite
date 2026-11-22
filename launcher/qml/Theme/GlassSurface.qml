// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// The surface, the hairline border, a 1 px light line along the top edge with
// a faint sheen fading down from it and, given a `level`, Theme's Shadow
// underneath. Only the sidebar, the play bar and popups/dialogs use it
// (Theme.glass). Popups take the default fill; the sidebar and play bar set
// `color: Theme.glass.fill` and usually no level.
Rectangle {
    id: glass

    // Theme.elevation.sm/md/lg, or null for no shadow.
    property QtObject level: null

    color: Theme.glass.popup
    border.width: 1
    border.color: Theme.glass.border

    Shadow {
        visible: glass.level !== null
        radius: glass.radius
        level: glass.level || Theme.elevation.md
    }

}
