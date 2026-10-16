// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The font the game console (the log view) is drawn in: a box listing the
 * fonts installed on this computer. Stored by family name in ConsoleFont,
 * where LogTab and the classic console read it. A family that is no longer
 * installed still shows, so a setting from another computer is not lost.
 */
SettingRow {
    id: root

    property string key: "ConsoleFont"
    property var source: SettingsStore
    readonly property string stored: root.source.string(root.key)
    // Installed families, plus the stored one if it is not among them.
    readonly property var families: {
        var list = Qt.fontFamilies()
        if (root.stored.length > 0 && list.indexOf(root.stored) < 0)
            list = [root.stored].concat(list)
        return list
    }

    ComboBox {
        id: combo
        width: Theme.control.heightLg * 6
        model: root.families
        Accessible.name: root.label

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
