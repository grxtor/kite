// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The paste.ee key used for uploaded logs -- the QML equivalent of the
 * classic PasteEEPage, which only ever has this one setting (PasteEEAPIKey:
 * either the literal string "meshmc", or a key of the account's own).
 *
 * `ownKey` is local UI state, not a live binding to the stored setting: it
 * only tracks which radio is selected, synced from the setting once at
 * startup and from then on changed only by picking a radio here. Deriving
 * it from the setting on every read instead would snap the "own key" radio
 * back to "MeshMC key" the moment it is picked but before a key has been
 * typed in, since the setting itself has not changed yet.
 */
SettingsScroll {
    id: root

    title: qsTr("Log upload")
    description: qsTr("paste.ee is used to share uploaded logs. Add your own API key to have uploads paired with your paste.ee account.")

    readonly property string meshmcKey: "meshmc"
    property bool ownKey: SettingsStore.string("PasteEEAPIKey") !== root.meshmcKey

    SettingsGroup {
        width: parent.width
        title: qsTr("API key")
        description: qsTr("Never shown again once set. Retype it to change it.")

        ButtonGroup { id: keyGroup }

        SettingRow {
            wide: true
            label: qsTr("MeshMC key")
            description: qsTr("12 MB upload limit.")
            RadioButton {
                ButtonGroup.group: keyGroup
                checked: !root.ownKey
                text: qsTr("Use the MeshMC key")
                onToggled: if (checked) {
                    root.ownKey = false
                    SettingsStore.setValue("PasteEEAPIKey", root.meshmcKey)
                }
            }
        }
        SettingRow {
            wide: true
            label: qsTr("Your own key")
            description: qsTr("12 MB upload limit. Get one at paste.ee.")
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.xs
                RadioButton {
                    ButtonGroup.group: keyGroup
                    checked: root.ownKey
                    text: qsTr("Use my own key")
                    onToggled: if (checked) root.ownKey = true
                }
                TextField {
                    Layout.fillWidth: true
                    visible: root.ownKey
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Paste your API key here")
                    selectByMouse: true
                    onEditingFinished: if (text.trim().length > 0)
                                           SettingsStore.setValue("PasteEEAPIKey", text.trim())
                }
            }
        }
    }
}
