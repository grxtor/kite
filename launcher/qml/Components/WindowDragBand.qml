// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// macOS, title bar laid over the content: dragging this band moves the
// window and a double click zooms it.
Item {
    DragHandler {
        target: null
        onActiveChanged: if (active) Window.window.startSystemMove()
    }
    TapHandler {
        onDoubleTapped: Window.window.visibility === Window.Maximized ? Window.window.showNormal()
                                                                      : Window.window.showMaximized()
    }
}
