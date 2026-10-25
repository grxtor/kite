// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Friends" section of SettingsPage.qml: the switch that turns Friends
 * on and off (on unless it was turned off here), who may see you, what they
 * see of where you play and whether they can join you there, and deleting
 * what Kite's server holds about you. The privacy rows after the switch are
 * dimmed while it is off, and stay out of reach (with no values) until the
 * server has said what the account's settings are; deleting is not, since
 * turning Friends on only to
 * delete is exactly what someone who turned it off does not want (Kite signs
 * in for that one request and leaves Friends off). It stays out of the list
 * where the shell has no friends controller.
 */
SettingsScroll {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // The default Minecraft account if it is a Microsoft one, "" otherwise:
    // Friends keeps a separate account for each, and Delete removes this one.
    property string accountName: ""
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: !!root.controller
    readonly property bool on: !!root.controller && root.controller.enabled
    // Friends is on and the account's settings have been read: before that
    // the controller holds the defaults, which are not what the server has.
    readonly property bool settingsRead: root.on && root.controller.settingsLoaded

    // Appearing offline: nothing about where you are is shown or joined.
    readonly property bool invisible: privacy.string("visibility") === "invisible"
    readonly property bool serverHidden: privacy.bool("hideServer")

    title: qsTr("Friends")
    description: qsTr("See which server your friends are on, and join them when they let you. While Friends is off, Kite sends nothing.")

    // The privacy rows keep their values on the server, not in
    // SettingsStore; this lets SettingChoice and SettingSwitch read and
    // write them as they would a launcher setting.
    QtObject {
        id: privacy
        // What was just asked for, shown until the server has answered: a
        // switch that sat on the old value for the whole round trip (or
        // jumped back and forth) would look like it did nothing.
        property var pending: ({})
        property int inFlight: 0
        function value(key) {
            if (privacy.pending[key] !== undefined)
                return privacy.pending[key]
            // On, but not read yet: no value, rather than the defaults.
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
            // Changing what friends see makes the card above stale.
            SettingsStore.setValue("FriendsNoticeSeen", true)
            root.controller.setSettings(change)
        }
    }
    Connections {
        target: root.controller
        // The answer (or the refusal) makes the server's value the truth again.
        function onActionFinished(action) {
            if (action === "setSettings" && --privacy.inFlight <= 0) {
                privacy.inFlight = 0
                privacy.pending = ({})
            }
        }
        // Turned off with a save out: its answer is dropped.
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
                    // Follows the controller, which answers a moment later
                    // (signing in); toggling dropped the binding.
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
                // The name ends on the column's edge; the glyph is before it.
                glyphFirst: true
                value: root.controller ? root.controller.myName : ""
                emptyText: qsTr("Not signed in")
                textColor: Theme.palette.textSecondary
            }
        }
    }

    SettingsGroup {
        width: parent.width
        // Off, the rows are shown dimmed as they will be; on, they wait for
        // the server's answer.
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
            // Invisible hides the server along with everything else.
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
        // Joining needs the server shown, so it waits for "Hide which
        // server I am on" to be off and for you to be seen at all, and reads
        // as off until then.
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
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Your data")
        SettingRow {
            label: qsTr("Delete my Friends data")
            description: qsTr("Removes your friends, requests, blocks and settings from Kite's server.")
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
        // Off, it stays off: only turned off here when it was on. It is the
        // account that is the default now whose data goes, also with Friends
        // off, when nothing else names it.
        text: (root.on ? qsTr("Your friends, requests, blocks and settings are deleted from Kite's server, and Friends is turned off. This cannot be undone.")
                       : qsTr("Your friends, requests, blocks and settings are deleted from Kite's server. Friends stays off. This cannot be undone."))
              + (root.accountName.length > 0 ? "\n\n" + qsTr("This is the data of %1. Your other Minecraft accounts keep theirs.").arg(root.accountName) : "")
        onConfirmed: root.controller.deleteAccount()
    }
}
