// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * Instance backups (zip snapshots of the instance folder). Create/restore/export/import run off the GUI thread
 * behind a TaskWatcher (BackupController); one progress row shows whichever is in flight.
 */
Item {
    id: root

    // InstanceDetails.backups (BackupController).
    property var controller: null
    readonly property var model: root.controller ? root.controller.model : null
    readonly property int count: list.count
    readonly property bool running: !!root.controller && root.controller.running

    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    // Set when an action refuses to start (restoreBackup() returns null while the instance runs): the watcher stays
    // null, so this is how the tab says why nothing happened.
    property string actionError: ""

    // timestampText is "yyyy-MM-dd HH:mm:ss" local time; read back into a date.
    function backupAge(timestampText) {
        var ms = new Date(String(timestampText).replace(" ", "T")).getTime()
        return isNaN(ms) ? timestampText : Format.lastPlayed(ms)
    }
    // Whether a backup has no label and is named after its creation time ("2026-09-18_09-00-00.zip").
    function namedByTime(name) {
        return /^\d{4}-\d{2}-\d{2}[_ T-]\d{2}[-.:]\d{2}[-.:]\d{2}$/.test(String(name).replace(/\.zip$/i, ""))
    }
    // One named after its creation time reads as that date; a labelled one keeps its label; ".zip" is never shown.
    function backupTitle(name, timestampText) {
        var bare = String(name).replace(/\.zip$/i, "")
        if (!root.namedByTime(name))
            return bare
        var when = new Date(String(timestampText).replace(" ", "T"))
        return isNaN(when.getTime()) ? bare
               : qsTr("%1, %2").arg(Format.calendarDate(when)).arg(when.toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
    }
    function confirmRestore(row, name) {
        restoreConfirm.row = row
        restoreConfirm.text = qsTr("Replace this instance's current contents with the backup “%1”? This cannot be undone.").arg(name)
        restoreConfirm.open()
    }

    // A different instance's controller: what this tab was doing belonged to the previous one.
    onControllerChanged: {
        root.watcher = null
        root.actionError = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        TabToolbar {
            Layout.fillWidth: true

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

        NoticeBar {
            Layout.fillWidth: true
            visible: root.running
            text: qsTr("The game is running. Backups can be restored once it has closed.")
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

            delegate: TabListRow {
                id: row
                required property int index
                required property string name
                required property string timestampText
                required property string sizeText

                width: list.width
                title: root.backupTitle(row.name, row.timestampText)
                meta: root.namedByTime(row.name) ? row.sizeText
                                                 : [root.backupAge(row.timestampText), row.sizeText].join("  •  ")
                thumb.glyph: "archive"
                onMenuRequested: if (!root.busy) rowMenu.popup()

                Button {
                    text: qsTr("Restore")
                    enabled: !root.busy && !root.running
                    ToolTip.visible: hovered && root.running
                    ToolTip.text: qsTr("Close the game before restoring a backup.")
                    onClicked: root.confirmRestore(row.index, row.title)
                }
                IconButton {
                    id: moreButton
                    enabled: !root.busy
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)
                }

                Menu {
                    id: rowMenu
                    MenuItem {
                        text: qsTr("Export…")
                        icon.source: Icons.url("download")
                        onTriggered: {
                            exportDialog.row = row.index
                            exportDialog.open()
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Delete…")
                        icon.source: Icons.url("trash")
                        onTriggered: {
                            deleteConfirm.row = row.index
                            deleteConfirm.text = qsTr("Delete the backup “%1”? This cannot be undone.").arg(row.title)
                            deleteConfirm.open()
                        }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: list
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No backups yet")
        body: qsTr("A backup is a zip of this whole instance, including its worlds and mods, that you can restore later.")
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
