// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * The "General" section of SettingsPage.qml: how the library sorts, what a
 * new instance starts out with, where MeshMC keeps its folders (and which
 * other folders it looks in for instances), and the news notices.
 */
SettingsScroll {
    id: root

    // Show a folder in the file manager; SettingsPage passes it up to Main.qml.
    signal openPathRequested(string path)

    title: qsTr("General")
    description: qsTr("How the library sorts, where new instances start out, and where MeshMC keeps its folders.")
    SettingsGroup {
        width: parent.width
        title: qsTr("Library")
        SettingChoice {
            key: "InstSortMode"
            label: qsTr("Sort instances by")
            description: qsTr("Order inside each group of the library.")
            options: [
                { value: "Name", label: qsTr("Name") },
                { value: "LastLaunch", label: qsTr("Last played") }
            ]
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("New instances")
        SettingSwitch {
            key: "DownloadGameFilesDuringInstanceCreation"
            label: qsTr("Download game files right away")
            description: qsTr("Fetch libraries and assets while the instance is created, instead of on its first launch.")
        }
        SettingSwitch {
            key: "SkipModpackUpdatePrompt"
            invert: true
            label: qsTr("Offer to update an installed modpack")
            description: qsTr("Installing a modpack you already have suggests updating that instance instead of making a second copy.")
        }
        SettingSwitch {
            key: "BackupBeforeLaunch"
            label: qsTr("Back up before every launch")
            description: qsTr("Saves a backup of the instance, shown in its Backups tab, just before the game starts.")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Folders")
        description: qsTr("Relative paths are inside MeshMC's data folder. Changing a folder does not move what is already in it.")
        Repeater {
            model: [
                { key: "InstanceDir", label: qsTr("Instances") },
                { key: "CentralModsDir", label: qsTr("Mods") },
                { key: "IconsDir", label: qsTr("Icons") },
                { key: "SkinsDir", label: qsTr("Skins") },
                { key: "JavaDir", label: qsTr("Java runtimes") }
            ]
            delegate: FolderRow {
                required property var modelData
                key: modelData.key
                label: modelData.label
                onOpenRequested: (path) => root.openPathRequested(path)
            }
        }
    }
    ExtraInstanceFolders {
        width: parent.width
        onOpenRequested: (path) => root.openPathRequested(path)
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Notifications")
        SettingRow {
            label: qsTr("Show notices again")
            description: qsTr("News and announcements you have already seen come back the next time MeshMC checks.")
            enabled: SettingsStore.string("ShownNotifications").length > 0
            Button {
                text: qsTr("Reset")
                onClicked: SettingsStore.setValue("ShownNotifications", "")
            }
        }
    }
}
