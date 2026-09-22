// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

// An on/off setting, saved the moment it is flipped. `invert` is for
// settings stored as the opposite of what reads naturally in the UI.
SettingRow {
    id: root

    property string key
    // Where the value lives; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    property bool invert: false
    readonly property bool checked: root.invert !== root.source.bool(root.key)
    // After the new value is stored; `on` is what the switch now shows.
    signal switched(bool on)

    Switch {
        id: control
        checked: root.checked
        Accessible.name: root.label
        onToggled: {
            root.source.setValue(root.key, root.invert ? !checked : checked)
            // Toggling assigns `checked` and drops the binding; put it back
            // so a change made elsewhere still shows here.
            checked = Qt.binding(() => root.checked)
            root.switched(checked)
        }
    }
}
