// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * A path setting with a browse button, for the external tool installs and
 * executables under Settings > External tools. `folder` picks a directory
 * (JProfiler/MCEdit installs), otherwise one file. `toolId`, when set, adds a
 * Check button that runs SettingsAdapter::checkExternalTool() and shows the
 * result underneath.
 */
SettingRow {
    id: root

    property string key
    property var source: SettingsStore
    property bool folder: false
    property string placeholder
    property var nameFilters: [qsTr("All files (*)")]
    property string toolId: ""
    wide: true

    property string checkResult: ""
    property bool checkOk: false

    ColumnLayout {
        width: parent.width
        spacing: Theme.space.xs

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            TextField {
                id: field
                Layout.fillWidth: true
                placeholderText: root.placeholder
                selectByMouse: true
                font.family: Theme.font.mono
                Accessible.name: root.label

                readonly property string stored: root.source.string(root.key)
                onStoredChanged: if (!activeFocus) text = stored
                Component.onCompleted: text = stored
                onEditingFinished: if (text !== stored) root.source.setValue(root.key, text)
            }
            IconButton {
                iconName: "folder"
                tip: qsTr("Browse…")
                flat: false
                onClicked: root.folder ? folderDialog.open() : fileDialog.open()
            }
            Button {
                visible: root.toolId.length > 0
                text: qsTr("Check")
                onClicked: {
                    const adapter = root.source.adapter
                    const error = adapter ? adapter.checkExternalTool(root.toolId, field.text)
                                          : qsTr("Not available.")
                    root.checkResult = error.length === 0 ? qsTr("Looks good.") : error
                    root.checkOk = error.length === 0
                }
            }
        }

        Copy {
            visible: root.checkResult.length > 0
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: root.checkResult
            role: "caption"
            tone: root.checkOk ? "success" : "danger"
        }
    }

    FolderDialog {
        id: folderDialog
        currentFolder: field.text.length > 0 ? Format.fileUrl(field.text) : ""
        onAccepted: {
            field.text = Format.localPath(selectedFolder)
            root.source.setValue(root.key, field.text)
        }
    }
    FileDialog {
        id: fileDialog
        nameFilters: root.nameFilters
        onAccepted: {
            field.text = Format.localPath(selectedFile)
            root.source.setValue(root.key, field.text)
        }
    }
}
