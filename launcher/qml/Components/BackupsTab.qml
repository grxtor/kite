// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Instance backups (zip snapshots of the whole instance folder) -- the
 * widget-free replacement for BackupPage. Create/restore/export/import all
 * run off the GUI thread behind a TaskWatcher (see BackupController), so
 * this tab shows one progress row for whichever of them is currently in
 * flight rather than blocking.
 */
Item {
    id: root

    // InstanceDetails.backups (BackupController).
    property var controller: null
    readonly property var model: root.controller ? root.controller.model : null
    readonly property int count: list.count
    readonly property bool running: !!root.controller && root.controller.running

    // The TaskWatcher of whichever action is currently running, if any.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    // Set when an action call refuses to even start (restoreBackup()
    // returning null because the instance is running) - the watcher stays
    // null in that case, so this is the only way the tab has to tell the
    // user why nothing happened.
    property string actionError: ""

    // A different instance's controller: whatever this tab was doing
    // belonged to the previous one.
    onControllerChanged: {
        root.watcher = null
        root.actionError = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Text {
                Layout.fillWidth: true
                text: root.running
                      ? qsTr("The game is running; backups can be restored once it has closed.")
                      : list.count > 0 ? qsTr("%1 backups").arg(list.count) : ""
                color: root.running ? Theme.palette.warning : Theme.palette.textTertiary
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                enabled: !root.busy
                text: qsTr("Import…")
                icon.source: Icons.url("download")
                onClicked: importDialog.open()
            }
            Button {
                enabled: !root.busy
                text: qsTr("Create backup")
                icon.source: Icons.url("archive")
                onClicked: labelPrompt.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.busy || root.actionError.length > 0 || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.md

            Text {
                Layout.preferredWidth: 140
                text: root.actionError.length > 0 ? qsTr("Restore") : root.watcher ? (root.watcher.title || qsTr("Working…")) : ""
                color: root.actionError.length > 0 || (root.watcher && root.watcher.failed) ? Theme.palette.danger : Theme.palette.textSecondary
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Text {
                Layout.fillWidth: true
                text: root.actionError.length > 0
                      ? root.actionError
                      : root.watcher
                        ? (root.watcher.failed ? (root.watcher.error || qsTr("Failed."))
                                               : (root.watcher.status || qsTr("Working…")))
                        : ""
                color: root.actionError.length > 0 || (root.watcher && root.watcher.failed) ? Theme.palette.danger : Theme.palette.textSecondary
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            LaunchProgressBar {
                Layout.preferredWidth: 160
                visible: root.busy
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string timestampText
                required property string sizeText

                width: list.width - Theme.space.md
                height: Theme.control.heightLg + Theme.space.md
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.md
                    spacing: Theme.space.md

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Theme.radius.md
                        color: Theme.palette.surfaceSunken
                        MeshIcon { anchors.centerIn: parent; iconName: "archive"; size: Theme.icon.sm; color: Theme.palette.textTertiary }
                    }

                    Column {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1
                        Text {
                            width: parent.width
                            text: row.name
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.Medium
                        }
                        Text {
                            width: parent.width
                            text: qsTr("%1 · %2").arg(row.timestampText).arg(row.sizeText)
                            elide: Text.ElideRight
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }

                    IconButton {
                        iconName: "download"
                        tip: qsTr("Export…")
                        enabled: !root.busy
                        onClicked: {
                            exportDialog.row = row.index
                            exportDialog.open()
                        }
                    }
                    IconButton {
                        iconName: "refresh"
                        tip: root.running ? qsTr("Close the game before restoring a backup.") : qsTr("Restore")
                        enabled: !root.busy && !root.running
                        onClicked: {
                            restoreConfirm.row = row.index
                            restoreConfirm.text = qsTr("Replace this instance's current contents with the backup “%1”? This cannot be undone.").arg(row.name)
                            restoreConfirm.open()
                        }
                    }
                    IconButton {
                        iconName: "trash"
                        tip: qsTr("Delete")
                        enabled: !root.busy
                        onClicked: {
                            deleteConfirm.row = row.index
                            deleteConfirm.text = qsTr("Delete the backup “%1”? This cannot be undone.").arg(row.name)
                            deleteConfirm.open()
                        }
                    }
                }
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No backups yet")
        body: qsTr("A backup is a zip of this whole instance, including its worlds and mods, that you can restore later.")
        actionText: qsTr("Create backup")
        onActionTriggered: labelPrompt.open()
        MeshIcon { iconName: "archive"; size: 40; color: Theme.palette.textTertiary }
    }

    PromptDialog {
        id: labelPrompt
        title: qsTr("Create backup")
        label: qsTr("Optional label")
        placeholder: qsTr("e.g. before updating mods")
        confirmText: qsTr("Create")
        allowEmpty: true
        onSubmitted: (text) => {
            if (root.controller) {
                root.actionError = ""
                root.watcher = root.controller.createBackup(text)
            }
            close()
        }
    }

    ConfirmDialog {
        id: restoreConfirm
        property int row: -1
        title: qsTr("Restore backup")
        confirmText: qsTr("Restore")
        onConfirmed: {
            if (!root.controller)
                return
            root.actionError = ""
            var watcher = root.controller.restoreBackup(row)
            if (watcher)
                root.watcher = watcher
            else
                root.actionError = qsTr("Close the game before restoring a backup.")
        }
    }

    ConfirmDialog {
        id: deleteConfirm
        property int row: -1
        title: qsTr("Delete backup")
        confirmText: qsTr("Delete")
        onConfirmed: if (root.controller) {
            root.actionError = ""
            root.watcher = root.controller.deleteBackup(row)
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import backup")
        nameFilters: [qsTr("Zip files (*.zip)"), qsTr("All files (*)")]
        onAccepted: if (root.controller) {
            root.actionError = ""
            root.watcher = root.controller.importBackup(selectedFile.toString(), "")
        }
    }

    FileDialog {
        id: exportDialog
        property int row: -1
        title: qsTr("Export backup")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Zip files (*.zip)")]
        onAccepted: if (root.controller) {
            root.actionError = ""
            root.watcher = root.controller.exportBackup(row, selectedFile.toString())
        }
    }
}
