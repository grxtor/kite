// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One options.txt entry on GameOptionsTab, with the control the game's own Options screen uses. `spec` says which
 * (see GameOptionsTab's `optionGroups`):
 *
 *   kind "slider"  from/to/step in the shown unit, toSlider(raw) -> number,
 *                  toRaw(number) -> string, text(number) -> label
 *   kind "switch"  "true"/"false"
 *   kind "choice"  choices: [{ value, label }]
 *   kind "text"    text() -> label; read-only
 *
 * A change is written with GameOptions.setValue() at once (a slider when let go); the controls then re-bind to what
 * the file says, so a refused write shows the old value. A value the control cannot represent (another game
 * version's format, a GUI scale past the list) is shown as written, read-only.
 */
SettingRow {
    id: root

    property var spec: ({})
    property string raw: ""
    property var store: null
    property bool editable: false

    // A fixed-width slot at the slider's right end, so "8 chunks" growing to "32 chunks" never shifts it.
    readonly property int sliderWidth: 220
    readonly property int valueWidth: 84
    readonly property int choiceWidth: 160

    readonly property bool understood: spec.kind === "slider" ? raw !== "" && !isNaN(Number(raw))
                                     : spec.kind === "switch" ? raw === "true" || raw === "false"
                                     : spec.kind === "choice" ? spec.choices.some((c) => c.value === raw)
                                     : true
    readonly property bool interactive: understood && spec.kind !== "text"

    label: spec.label || ""
    enabled: editable || !interactive

    function commit(value) {
        if (root.store && value !== root.raw)
            root.store.setValue(root.spec.key, value)
    }

    Loader {
        sourceComponent: !root.interactive ? textControl
                       : root.spec.kind === "slider" ? sliderControl
                       : root.spec.kind === "switch" ? switchControl
                       : choiceControl
    }

    Component {
        id: sliderControl
        Row {
            spacing: Theme.space.md

            Slider {
                id: slider
                anchors.verticalCenter: parent.verticalCenter
                width: root.sliderWidth
                from: root.spec.from
                to: root.spec.to
                stepSize: root.spec.step
                snapMode: Slider.SnapAlways
                value: root.spec.toSlider(root.raw)
                Accessible.name: root.label
                function save() {
                    root.commit(root.spec.toRaw(Math.round(value)))
                    value = Qt.binding(() => root.spec.toSlider(root.raw))
                }
                onPressedChanged: if (!pressed) save()
                onMoved: if (!pressed) save()
            }

            Copy {
                anchors.verticalCenter: parent.verticalCenter
                width: root.valueWidth
                horizontalAlignment: Text.AlignRight
                text: root.spec.text(Math.round(slider.value))
                elide: Text.ElideRight
                role: "bodyStrong"
            }
        }
    }

    Component {
        id: switchControl
        Switch {
            checked: root.raw === "true"
            Accessible.name: root.label
            onToggled: {
                root.commit(checked ? "true" : "false")
                checked = Qt.binding(() => root.raw === "true")
            }
        }
    }

    Component {
        id: choiceControl
        ComboBox {
            width: root.choiceWidth
            model: root.spec.choices
            textRole: "label"
            valueRole: "value"
            // Through `count`, so it is worked out again once the model
            // has arrived rather than once, before it.
            currentIndex: count > 0 ? indexOfValue(root.raw) : -1
            Accessible.name: root.label
            onActivated: {
                root.commit(currentValue)
                currentIndex = Qt.binding(() => count > 0 ? indexOfValue(root.raw) : -1)
            }
        }
    }

    Component {
        id: textControl
        Copy {
            text: root.spec.kind === "text" ? root.spec.text() : root.raw
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            tone: "secondary"
        }
    }
}
