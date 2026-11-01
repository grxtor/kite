// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The paste.ee key used for uploaded logs (PasteEEAPIKey: either the literal
 * string "kite", or a key of the account's own). `ownKey` is local UI state, not
 * a live binding: deriving it from the setting would snap the "own key" radio
 * back to "Kite key" the moment it is picked, before a key is typed.
 */
SettingsScroll {
    id: root

    title: qsTr("Log upload")
    description: qsTr("paste.ee is used to share uploaded logs. Add your own API key to have uploads paired with your paste.ee account.")

    readonly property string kiteKey: "kite"
    property bool ownKey: SettingsStore.string("PasteEEAPIKey") !== root.kiteKey

    SettingsGroup {
        width: parent.width
        title: qsTr("API key")
        description: qsTr("Never shown again once set. Retype it to change it.")

        ButtonGroup { id: keyGroup }

        SettingRow {
            wide: true
            label: qsTr("Upload with")
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.xs
                RadioButton {
                    ButtonGroup.group: keyGroup
                    checked: !root.ownKey
                    text: qsTr("Kite's shared key (12 MB limit)")
                    onToggled: if (checked) {
                        root.ownKey = false
                        SettingsStore.setValue("PasteEEAPIKey", root.kiteKey)
                    }
                }
                RadioButton {
                    ButtonGroup.group: keyGroup
                    checked: root.ownKey
                    text: qsTr("My own key (12 MB limit, get one at paste.ee)")
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
