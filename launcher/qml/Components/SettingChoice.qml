// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A setting with a handful of named values: a drop-down by default, the
 * way every launcher shows "sort by" or "theme", or a column of radio
 * buttons (`radio: true`) when the choices change what the rows below show
 * (the proxy type) and are worth seeing all at once.
 */
SettingRow {
    id: root

    property string key
    // Where the value lives; the launcher-wide settings unless told otherwise.
    property var source: SettingsStore
    // [{ value, label }]
    property var options: []
    // Radio buttons instead of the drop-down.
    property bool radio: false
    signal changed(var value)

    readonly property int comboWidth: 200
    // Radio buttons sit under the label, left-aligned, not off at the right.
    wide: root.radio
    readonly property var stored: root.source.string(root.key)

    function choose(value) {
        root.source.setValue(root.key, value)
        root.changed(value)
    }

    Loader {
        sourceComponent: root.radio ? radioColumn : combo
    }

    Component {
        id: combo
        ComboBox {
            width: root.comboWidth
            model: root.options
            textRole: "label"
            valueRole: "value"
            currentIndex: indexOfValue(root.stored)
            onActivated: root.choose(currentValue)
            Accessible.name: root.label
        }
    }

    Component {
        id: radioColumn
        Column {
            spacing: Theme.space.xxs
            Repeater {
                model: root.options
                delegate: RadioButton {
                    required property var modelData
                    text: modelData.label
                    checked: root.stored === modelData.value
                    onClicked: root.choose(modelData.value)
                }
            }
        }
    }
}
