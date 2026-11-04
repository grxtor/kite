// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Friends" section of SettingsPage.qml: the Friends switch, desktop
 * notifications, who may see you, where you play and who may invite you,
 * your share codes, and deleting what Kite's server holds about you. The privacy rows are dimmed while it is off and out
 * of reach until the server has sent the account's settings; deleting stays
 * available, since turning Friends on only to delete is what someone who
 * turned it off does not want.
 */
SettingsScroll {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // QmlShell.shareCodes (ShareCodesController), or null.
    property var shareCodes: null
    // The default Minecraft account if it is a Microsoft one, "" otherwise:
    // Friends keeps a separate account for each, and Delete removes this one.
    property string accountName: ""
    readonly property bool shown: !!root.controller
    readonly property bool on: !!root.controller && root.controller.enabled
    // Friends is on and the account's settings are read: before that the controller holds defaults,
    // not what the server has.
    readonly property bool settingsRead: root.on && root.controller.settingsLoaded

    readonly property bool invisible: privacy.string("visibility") === "invisible"
    readonly property bool serverHidden: privacy.bool("hideServer")

    title: qsTr("Friends")
    description: qsTr("See which server your friends are on, and join them when they let you. While Friends is off, Kite sends nothing.")

    // The privacy rows keep their values on the server; this lets SettingChoice and SettingSwitch
    // read and write them like launcher settings.
    QtObject {
        id: privacy
        // What was just asked for, shown until the server answers, so a switch does not look like
        // it did nothing.
        property var pending: ({})
        property int inFlight: 0
        function value(key) {
            if (privacy.pending[key] !== undefined)
                return privacy.pending[key]
            if (root.on && !root.settingsRead)
                return undefined
            return root.controller ? root.controller.settings[key] : undefined
        }
        function string(key) { return String(privacy.value(key) || "") }
        function bool(key) { return !!privacy.value(key) }
        function setValue(key, value) {
            var change = {}
            change[key] = value
            var asked = Object.assign({}, privacy.pending)
            asked[key] = value
            privacy.pending = asked
            privacy.inFlight++
            SettingsStore.setValue("FriendsNoticeSeen", true)
            root.controller.setSettings(change)
        }
    }
    Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "setSettings" && --privacy.inFlight <= 0) {
                privacy.inFlight = 0
                privacy.pending = ({})
            }
        }
        function onChanged() {
            if (!root.on && privacy.inFlight > 0) {
                privacy.inFlight = 0
                privacy.pending = ({})
            }
        }
    }

    SettingsGroup {
        width: parent.width
        SettingRow {
            label: qsTr("Use Friends")
            description: qsTr("Shows your friends and lets them see you.")
            Switch {
                checked: root.on
                Accessible.name: qsTr("Use Friends")
                onToggled: {
                    const turnOn = checked
                    checked = Qt.binding(() => root.on)
                    root.controller.enabled = turnOn
                }
            }
        }
        SettingRow {
            enabled: root.on
            label: qsTr("Your Minecraft name")
            description: qsTr("Friends add you by this Minecraft name.")
            CopyName {
                glyphFirst: true
                value: root.controller ? root.controller.myName : ""
                emptyText: qsTr("Not signed in")
                textColor: Theme.palette.textSecondary
            }
        }
        // A launcher setting, not one of the server's: the notices are shown by the DesktopNotifier plugin.
        SettingSwitch {
            enabled: root.on
            key: "FriendsNotifications"
            label: qsTr("Desktop notifications")
            description: qsTr("Tells you when a friend comes online, sends you a friend request or invites you to a server.")
        }
        SettingSwitch {
            enabled: root.on
            key: "FriendsMessageNotifications"
            label: qsTr("Message notifications")
            description: qsTr("Tells you when a friend sends you a message while their chat is not on screen.")
        }
    }

    SettingsGroup {
        width: parent.width
        enabled: root.settingsRead
        title: qsTr("Privacy")
        description: root.on && !root.settingsRead
                     ? qsTr("These show once Kite's friends server has answered.") : ""
        SettingChoice {
            source: privacy
            key: "visibility"
            label: qsTr("Who can see me")
            description: qsTr("Friends see when you are online. With Everyone, any Kite user who knows your name sees it too. With Invisible, you appear offline.")
            options: [
                { value: "friends", label: qsTr("Friends") },
                { value: "everyone", label: qsTr("Everyone") },
                { value: "invisible", label: qsTr("Invisible") }
            ]
        }
        SettingSwitch {
            source: privacy
            key: "hideServer"
            enabled: !root.invisible
            forcedOn: root.invisible
            label: qsTr("Hide which server I am on")
            description: root.invisible ? qsTr("Invisible hides it too.")
                                        : qsTr("Friends see that you are playing, but not where.")
        }
        SettingSwitch {
            source: privacy
            key: "shareIpServers"
            enabled: !root.invisible && !root.serverHidden
            label: qsTr("Show servers I join by IP address")
            description: root.invisible ? qsTr("Not while you are invisible.")
                       : root.serverHidden ? qsTr("Not while you hide which server you are on.")
                       : qsTr("Servers on your home network, or reached by a raw IP address, stay hidden unless this is on.")
        }
        // Joining needs the server shown: off until "Hide which server I am on" is off and you are
        // seen at all.
        SettingSwitch {
            source: privacy
            key: "allowJoin"
            enabled: !root.invisible && !root.serverHidden
            forcedOff: root.invisible || root.serverHidden
            label: qsTr("Let friends join the server I am on")
            description: root.invisible ? qsTr("Not while you are invisible.")
                       : root.serverHidden ? qsTr("Not while you hide which server you are on.")
                       : qsTr("A friend gets a Join button next to your name when your server is shown.")
        }
        SettingSwitch {
            source: privacy
            key: "allowInvites"
            label: qsTr("Let friends invite me to servers")
            description: qsTr("A friend's invite shows in Friends with a Join button for ten minutes. Turning this off also drops the invites you have.")
        }
    }

    ShareCodesGroup {
        width: parent.width
        controller: root.shareCodes
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Your data")
        SettingRow {
            label: qsTr("Delete my Friends data")
            description: qsTr("Removes your friends, requests, invites, share codes, blocks and settings from Kite's server.")
            Button {
                text: qsTr("Delete…")
                icon.source: Icons.url("trash")
                onClicked: deleteDialog.open()
            }
        }
    }

    ConfirmDialog {
        id: deleteDialog
        title: qsTr("Delete your Friends data?")
        confirmText: qsTr("Delete")
        // Deleting works with Friends off: it removes the data of the account that is the default now.
        text: (root.on ? qsTr("Your friends, requests, blocks and settings are deleted from Kite's server, and Friends is turned off. This cannot be undone.")
                       : qsTr("Your friends, requests, blocks and settings are deleted from Kite's server. Friends stays off. This cannot be undone."))
              + (root.accountName.length > 0 ? "\n\n" + qsTr("This is the data of %1. Your other Minecraft accounts keep theirs.").arg(root.accountName) : "")
        onConfirmed: root.controller.deleteAccount()
    }
}
