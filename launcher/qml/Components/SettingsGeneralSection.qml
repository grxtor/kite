// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "General" section of SettingsPage.qml: how the library sorts, what a
 * new instance starts out with, and where MeshMC keeps its folders.
 */
SettingsScroll {
    id: root

    // Show a folder in the file manager; SettingsPage passes it up to Main.qml.
    signal openPathRequested(string path)

    title: qsTr("General")
    description: qsTr("How the library sorts and where new instances start out.")
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
            description: qsTr("Keeps a “pre-launch” snapshot in the instance's .backups folder.")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Folders")
        description: qsTr("Relative paths are inside MeshMC's data folder. These folders can be opened from here, but not moved.")
        Repeater {
            model: [
                { key: "InstanceDir", label: qsTr("Instances") },
                { key: "CentralModsDir", label: qsTr("Mods") },
                { key: "IconsDir", label: qsTr("Icons") },
                { key: "SkinsDir", label: qsTr("Skins") },
                { key: "JavaDir", label: qsTr("Java runtimes") }
            ]
            delegate: SettingRow {
                required property var modelData
                label: modelData.label
                description: SettingsStore.string(modelData.key)
                IconButton {
                    iconName: "folder"
                    tip: qsTr("Open folder")
                    flat: false
                    onClicked: root.openPathRequested(SettingsStore.string(modelData.key))
                }
            }
        }
    }
}
