// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A font family setting: a box listing the fonts installed on this
 * computer. Stored by family name. A family that is no longer installed
 * still shows, so a setting from another computer is not lost, and
 * `defaultFamily` (when set) is listed first and named as the default.
 */
SettingRow {
    id: root

    property string key
    property var source: SettingsStore
    // Listed first and marked "(default)", e.g. "Inter" for the interface.
    property string defaultFamily: ""
    readonly property string stored: root.source.string(root.key).length > 0
                                     ? root.source.string(root.key) : root.defaultFamily
    // Installed families, the default and the stored one first.
    readonly property var families: {
        var list = Qt.fontFamilies()
        if (root.defaultFamily.length > 0) {
            list = list.filter(f => f !== root.defaultFamily)
            list = [root.defaultFamily].concat(list)
        }
        if (root.stored.length > 0 && list.indexOf(root.stored) < 0)
            list = [root.stored].concat(list)
        return list
    }

    ComboBox {
        id: combo
        width: Theme.control.heightLg * 6
        model: root.families
        Accessible.name: root.label
        displayText: currentText + (currentText === root.defaultFamily && root.defaultFamily.length > 0
                                    ? qsTr(" (default)") : "")
        delegate: ItemDelegate {
            required property var modelData
            required property int index
            width: combo.width
            text: modelData + (modelData === root.defaultFamily ? qsTr(" (default)") : "")
            highlighted: combo.highlightedIndex === index
            font.family: modelData
        }

        // See JavaVendorRow.qml for why the selection is set by a function
        // and not a binding.
        function sync() { currentIndex = count > 0 ? find(root.stored) : -1 }
        Connections {
            target: root
            function onStoredChanged() { combo.sync() }
        }
        onCountChanged: sync()
        Component.onCompleted: sync()
        onActivated: {
            root.source.setValue(root.key, currentText)
            sync()
        }
    }
}
