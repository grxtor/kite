// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// What a modal Popup or Dialog dims the window with. On macOS the title bar is
// part of the window, so the top band stays a window-drag handle; it sits above
// the overlay, which would otherwise take a press there as "outside" and close
// the popup.
Rectangle {
    id: scrim

    color: Theme.palette.scrim

    WindowDragZone {
        parent: T.Overlay.overlay ? T.Overlay.overlay.parent : null
        z: 2000000
        shown: scrim.visible
    }
}
