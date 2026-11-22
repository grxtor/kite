// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A provider's gallery pictures large, in the ScreenshotViewer, read-only: Left/Right and the arrows go through
 * them, Esc closes, and the one action opens the picture in the browser. show(images, index) takes the list a
 * GalleryStrip or grid shows ([{ url, rawUrl?, title? }]); `fallbackTitle` names a picture that has no title of its own.
 */
Item {
    id: root

    property string fallbackTitle: ""
    readonly property bool opened: viewer.opened

    function show(images, index) {
        pictures.clear()
        for (let i = 0; i < images.length; ++i)
            pictures.append({ name: images[i].title || "", path: "", url: images[i].rawUrl || images[i].url || "" })
        viewer.show(index)
    }
    function close() {
        viewer.close()
    }

    ListModel { id: pictures }

    // What the viewer asks of a ScreenshotsTab, as far as a read-only gallery has it.
    QtObject {
        id: source
        readonly property var model: pictures
        readonly property string coverSource: ""
        function captionOf(name) { return name.length > 0 ? name : root.fallbackTitle }
        function isCover(path) { return false }
    }

    ScreenshotViewer {
        id: viewer
        owner: source
        readOnly: true
    }
}
