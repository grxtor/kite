// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * One of MeshMC's folders (Settings > General > Folders): what it is, where
 * it is now, and buttons to pick another place or open it. The pick goes
 * through FolderSettingsController, which asks about risky locations
 * (a "!" in the path, a folder borrowed through Flatpak) before saving.
 * Existing contents are not moved.
 */
SettingRow {
    id: root

    // The folder setting: InstanceDir, CentralModsDir, IconsDir, ...
    property string key
    readonly property string path: SettingsStore.string(key)
    readonly property bool movable: !!LauncherStore.folders

    signal openRequested(string path)

    description: path

    Row {
        spacing: Theme.space.sm

        Button {
            visible: root.movable
            text: qsTr("Change…")
            onClicked: dialog.open()
        }
        IconButton {
            iconName: "folder"
            tip: qsTr("Open folder")
            flat: false
            onClicked: root.openRequested(root.path)
        }
    }

    FolderDialog {
        id: dialog
        title: root.label
        // Only an absolute path can be a starting point; a relative one is
        // inside the data folder, which the dialog cannot see.
        currentFolder: root.path.length > 0 && root.path.charAt(0) === "/" ? "file://" + root.path : ""
        onAccepted: LauncherStore.folders.setFolder(root.key, selectedFolder)
    }
}
