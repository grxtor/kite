// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * What a view does when its upload comes back: copy the link and say so in a toast, or show the error. Set
 * `pending` when this view starts an upload, so another view sharing the uploader does not announce it too.
 */
QtObject {
    id: root

    // A LogUploader or ScreenshotUploader: uploaded(url[, count]) / failed(message).
    property var uploader: null
    // The host's ClipboardText and Toast.
    property var clipboardItem: null
    property var toastItem: null
    property bool pending: false
    // The toast text for an upload of several files (count > 1), if the view has one.
    property string manyText: ""

    readonly property Connections replies: Connections {
        target: root.uploader
        ignoreUnknownSignals: true
        function onUploaded(url, count) {
            if (!root.pending)
                return
            root.pending = false
            root.clipboardItem.copyText(url)
            root.toastItem.show(count > 1 && root.manyText.length > 0 ? root.manyText
                                                                   : qsTr("Link copied to your clipboard."),
                            "success", qsTr("Open"), () => ExternalLinks.open(root.toastItem, url))
        }
        function onFailed(message) {
            if (!root.pending)
                return
            root.pending = false
            root.toastItem.show(message, "danger")
        }
    }
}
