// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

// A setting with a handful of named values, as a segmented control.
SettingRow {
    id: root

    property string key
    // Where the value lives; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    // [{ value, label }]
    property var options: []
    signal changed(var value)

    SegmentedControl {
        options: root.options
        current: root.source.string(root.key)
        onActivated: (value) => {
            root.source.setValue(root.key, value)
            root.changed(value)
        }
    }
}
