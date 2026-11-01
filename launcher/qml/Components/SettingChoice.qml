// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A setting with a handful of named values: a drop-down, or a column of radio
 * buttons (`radio: true`) when the choices change what the rows below show.
 */
SettingRow {
    id: root

    property string key
    property var source: SettingsStore
    property var options: []
    property bool radio: false
    signal changed(var value)

    readonly property int comboWidth: 200
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
            // Through `count`, so it is worked out again once the model has arrived.
            currentIndex: count > 0 ? indexOfValue(root.stored) : -1
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
