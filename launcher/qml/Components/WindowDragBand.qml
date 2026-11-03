// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// macOS, title bar laid over the content: dragging this band moves the
// window and a double click zooms it.
Item {
    id: band

    // The attached Window.window is null inside a handler (it is not an Item), so it is read through the band.
    DragHandler {
        target: null
        onActiveChanged: if (active && band.Window.window) band.Window.window.startSystemMove()
    }
    TapHandler {
        onDoubleTapped: {
            const window = band.Window.window
            if (!window)
                return
            if (window.visibility === Window.Maximized)
                window.showNormal()
            else
                window.showMaximized()
        }
    }
}
