// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The "Updates" section of SettingsPage.qml: automatic update checks, the release channel, when Kite last
 * looked and "Check now", and an update waiting for Kite to close. It hides itself while this build has no
 * updater. Where a package manager updates Kite, Kite only tells; the text says so.
 */
SettingsScroll {
    id: root

    readonly property var updates: LauncherStore.updates
    readonly property bool shown: !!updates && updates.available
    readonly property string managedBy: shown ? updates.managedBy : ""
    readonly property string pendingVersion: shown ? updates.pendingVersion : ""

    // "5 min ago" goes stale while the page is open; this moves it on.
    property int clock: 0
    Timer {
        interval: 60000
        repeat: true
        running: root.visible && root.shown
        onTriggered: root.clock++
    }

    function lastCheckedText() {
        root.clock
        var ms = root.shown ? root.updates.lastCheckedMs : 0
        return ms > 0 ? Format.lastPlayed(ms) : qsTr("Never")
    }

    title: qsTr("Updates")
    description: qsTr("Whether Kite looks for new versions by itself.")

    SettingsGroup {
        width: parent.width
        visible: root.pendingVersion.length > 0
        title: qsTr("Ready to install")
        SettingRow {
            objectName: "pendingUpdateRow"
            label: qsTr("Kite %1").arg(root.pendingVersion)
            description: root.shown && root.updates.canInstallNow
                         ? qsTr("Installs when you close Kite, or now with a restart.")
                         : qsTr("Installs when you close Kite. A game is running, so Kite cannot restart now.")
            Button {
                text: qsTr("Restart and update")
                highlighted: true
                enabled: root.shown && root.updates.canInstallNow
                onClicked: root.updates.installPendingNow()
            }
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Update checks")
        SettingRow {
            label: qsTr("Check for updates automatically")
            description: root.managedBy.length > 0
                         ? qsTr("When Kite starts and once a day. %1 installs updates on this computer; Kite tells you when one is out.").arg(root.managedBy)
                         : qsTr("When Kite starts and once a day. Kite asks before it installs anything.")
            Switch {
                checked: !!root.updates && root.updates.autoCheck
                Accessible.name: qsTr("Check for updates automatically")
                onToggled: {
                    root.updates.autoCheck = checked
                    checked = Qt.binding(() => !!root.updates && root.updates.autoCheck)
                }
            }
        }
        SettingRow {
            showDivider: true
            label: qsTr("Channel")
            description: qsTr("Finished releases only. Kite has no test builds to offer yet.")
            Copy {
                text: root.shown && root.updates.channel === "beta" ? qsTr("Beta") : qsTr("Stable")
                role: "label"
                tone: "secondary"
            }
        }
        SettingRow {
            objectName: "lastCheckedRow"
            showDivider: true
            label: qsTr("Last checked")
            description: root.lastCheckedText()
            Button {
                text: qsTr("Check now")
                icon.source: Icons.url("refresh")
                onClicked: root.updates.checkNow()
            }
        }
    }
}
