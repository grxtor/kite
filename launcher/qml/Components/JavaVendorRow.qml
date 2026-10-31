// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Which vendor automatic Java downloads come from (JavaAutoDownloadVendor). Hidden
 * in a build without the Java downloader and until the vendor list is available.
 */
SettingRow {
    id: root

    property string key: "JavaAutoDownloadVendor"
    property var source: SettingsStore
    readonly property var java: JavaStore.controller

    visible: !!java && java.downloaderAvailable

    ComboBox {
        width: Theme.control.heightLg * 6
        model: root.java ? root.java.vendors : []
        textRole: "label"
        valueRole: "value"
        Accessible.name: root.label

        // Selected by value, again whenever the list arrives or the setting changes: a binding on
        // currentIndex loses to the box's own "select the first row when the list fills" step.
        readonly property string stored: root.source.string(root.key)
        function sync() { currentIndex = count > 0 ? indexOfValue(stored) : -1 }
        onStoredChanged: sync()
        onCountChanged: sync()
        Component.onCompleted: sync()
        onActivated: {
            root.source.setValue(root.key, currentValue)
            sync()
        }
    }
}
