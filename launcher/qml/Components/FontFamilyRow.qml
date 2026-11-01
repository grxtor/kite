// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A font family setting: a box listing the fonts installed here, stored by family name. A family no longer installed
 * still shows, so a setting from another computer is not lost; `defaultFamily` (when set) is listed first.
 */
SettingRow {
    id: root

    property string key
    property var source: SettingsStore
    property string defaultFamily: ""
    readonly property string stored: root.source.string(root.key).length > 0
                                     ? root.source.string(root.key) : root.defaultFamily
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

        // The selection is set by a function, not a binding (see JavaVendorRow.qml).
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
