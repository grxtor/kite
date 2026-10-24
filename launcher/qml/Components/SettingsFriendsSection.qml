// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Friends" section of SettingsPage.qml: the switch that turns friends on
 * (after the question in FriendsOptInDialog), who may see you, what they see
 * of where you play, and deleting what Kite's server holds about you. The
 * rows after the switch are dimmed while it is off. It stays out of the list
 * where the shell has no friends controller.
 */
SettingsScroll {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: !!root.controller
    readonly property bool on: !!root.controller && root.controller.enabled

    title: qsTr("Friends")
    description: qsTr("See which server your friends are playing on. There is no chat. While friends are off, Kite sends nothing.")

    // The privacy rows keep their values on the server, not in
    // SettingsStore; this lets SettingChoice and SettingSwitch read and
    // write them as they would a launcher setting.
    QtObject {
        id: privacy
        function string(key) { return root.controller ? String(root.controller.settings[key] || "") : "" }
        function bool(key) { return !!root.controller && !!root.controller.settings[key] }
        function setValue(key, value) {
            var change = {}
            change[key] = value
            root.controller.setSettings(change)
        }
    }

    SettingsGroup {
        width: parent.width
        SettingRow {
            label: qsTr("Use friends")
            description: qsTr("Shows your friends and lets them see you.")
            Switch {
                checked: root.on
                Accessible.name: qsTr("Use friends")
                onToggled: {
                    // The switch turns on only once the question is answered.
                    checked = Qt.binding(() => root.on)
                    if (root.on)
                        root.controller.setEnabled(false)
                    else
                        optIn.open()
                }
            }
        }
        SettingRow {
            enabled: root.on
            label: qsTr("Signed in as")
            description: qsTr("Your friends find you by this Minecraft name.")
            Text {
                text: root.controller && root.controller.myName.length > 0 ? root.controller.myName : qsTr("Not signed in")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
        }
    }

    SettingsGroup {
        width: parent.width
        enabled: root.on
        title: qsTr("Privacy")
        SettingChoice {
            source: privacy
            key: "visibility"
            label: qsTr("Who can see you")
            description: qsTr("Friends see when you are online. Everyone also lets any Kite user find you by name. Invisible shows you as offline.")
            options: [
                { value: "friends", label: qsTr("Friends") },
                { value: "everyone", label: qsTr("Everyone") },
                { value: "invisible", label: qsTr("Invisible") }
            ]
        }
        SettingSwitch {
            source: privacy
            key: "hideServer"
            label: qsTr("Hide which server I am on")
            description: qsTr("Friends see that you are playing, but not where.")
        }
        SettingSwitch {
            source: privacy
            key: "shareIpServers"
            label: qsTr("Show servers I join by IP address")
            description: qsTr("Servers on your home network, or reached by a raw IP address, stay hidden unless this is on.")
        }
    }

    SettingsGroup {
        width: parent.width
        enabled: root.on
        title: qsTr("Your data")
        SettingRow {
            label: qsTr("Delete my friends data")
            description: qsTr("Removes your friends, requests, blocks and settings from Kite's server.")
            Button {
                text: qsTr("Delete…")
                icon.source: Icons.url("trash")
                onClicked: deleteDialog.open()
            }
        }
    }

    FriendsOptInDialog {
        id: optIn
        controller: root.controller
    }

    ConfirmDialog {
        id: deleteDialog
        title: qsTr("Delete your friends data?")
        confirmText: qsTr("Delete")
        text: qsTr("Your friends, requests, blocks and settings are deleted from Kite's server, and friends are turned off. This cannot be undone.")
        onConfirmed: root.controller.deleteAccount()
    }
}
