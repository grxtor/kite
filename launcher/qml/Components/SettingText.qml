// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A free-text setting. Saved when editing finishes (Enter or leaving the
 * field), not per keystroke, so half-typed paths never reach the config.
 */
SettingRow {
    id: root

    property string key
    // Where the value lives; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    property string placeholder
    property bool monospace: false
    property bool secret: false
    wide: true

    TextField {
        id: field
        width: parent ? parent.width : 320
        placeholderText: root.placeholder
        echoMode: root.secret ? TextInput.Password : TextInput.Normal
        font.family: root.monospace ? Theme.font.mono : Theme.font.family
        selectByMouse: true
        Accessible.name: root.label

        readonly property string stored: root.source.string(root.key)
        onStoredChanged: if (!activeFocus) text = stored
        Component.onCompleted: text = stored
        onEditingFinished: if (text !== stored) root.source.setValue(root.key, text)
    }
}
