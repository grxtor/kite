// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

// An on/off setting, saved the moment it is flipped. `invert` is for
// settings stored as the opposite of what reads naturally in the UI.
SettingRow {
    id: root

    property string key
    property var source: SettingsStore
    property bool invert: false
    // Shows the switch off whatever is stored, for a setting that cannot
    // take effect now (the stored value comes back with it).
    property bool forcedOff: false
    // The same for a setting that is in effect whatever is stored (another
    // setting already does what it would).
    property bool forcedOn: false
    readonly property bool checked: root.forcedOn || (!root.forcedOff && root.invert !== root.source.bool(root.key))
    signal switched(bool on)

    Switch {
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
