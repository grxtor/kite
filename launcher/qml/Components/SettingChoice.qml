// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// A setting with a handful of named values, as a segmented control.
SettingRow {
    id: root

    property string key
    // [{ value, label }]
    property var options: []
    signal changed(var value)

    SegmentedControl {
        options: root.options
        current: SettingsStore.string(root.key)
        onActivated: (value) => {
            SettingsStore.setValue(root.key, value)
            root.changed(value)
        }
    }
}
