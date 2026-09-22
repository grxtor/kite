// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

// A whole-number setting with bounds and a unit, saved on every change.
SettingRow {
    id: root

    property string key
    property int from: 0
    property int to: 100
    property int stepSize: 1
    property string suffix
    readonly property int current: SettingsStore.number(root.key)
    signal changed(int value)

    SpinBox {
        id: control
        width: 168
        from: root.from
        to: root.to
        stepSize: root.stepSize
        editable: true
        value: root.current
        textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0)
                                           + (root.suffix.length > 0 ? " " + root.suffix : "")
        valueFromText: (text, locale) => {
            var digits = text.replace(/[^0-9]/g, "")
            return digits.length > 0 ? parseInt(digits, 10) : root.current
        }
        Accessible.name: root.label
        onValueModified: {
            SettingsStore.setValue(root.key, value)
            root.changed(value)
            value = Qt.binding(() => root.current)
        }
    }
}
