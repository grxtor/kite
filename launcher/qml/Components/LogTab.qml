// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The game's output, live, plus (when the instance has any) its other log
 * files -- logs/*.log*, crash-reports/*.txt -- the widget-free replacement
 * for OtherLogsPage, folded into this tab rather than given its own: both
 * are "read a log", just live vs. on disk.
 *
 * "Live" follows the newest line only while "Follow" is on -- scrolling up
 * to read something turns it off, instead of the view yanking the reader
 * back down on every new line.
 */
Item {
    id: root

    // InstanceLogBridge: model (LogModel: line, level), hasLog, clear(),
    // text().
    property var log: null
    // OtherLogsModel (roles: name), or null - see InstanceDetails.otherLogs.
    property var otherLogs: null
    readonly property var model: log ? log.model : null

    signal openFolderRequested(string path)

    // "live" or "other".
    property string view: "live"

    // MessageLevel::Enum
    function levelColor(level) {
        switch (level) {
        case 2: case 8: case 9: return Theme.palette.danger   // StdErr, Error, Fatal
        case 7: return Theme.palette.warning                  // Warning
        case 3: return Theme.palette.accent                   // MeshMC
        case 4: return Theme.palette.textTertiary             // Debug
        default: return Theme.palette.textSecondary
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            SegmentedControl {
                visible: !!root.otherLogs
                options: [
                    { value: "live", label: qsTr("Live") },
                    { value: "other", label: qsTr("Other logs") }
                ]
                current: root.view
                onActivated: (value) => root.view = value
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                visible: root.view === "live"
                spacing: Theme.space.sm

                Switch {
                    id: follow
                    checked: true
                    text: qsTr("Follow")
                }
                Button {
                    enabled: !!root.model
                    text: qsTr("Copy all")
                    icon.source: Icons.url("copy")
                    onClicked: {
                        clipboardHelper.text = root.log.text()
                        clipboardHelper.selectAll()
                        clipboardHelper.copy()
                    }
                }
                Button {
                    enabled: !!root.model
                    flat: true
                    text: qsTr("Clear")
                    icon.source: Icons.url("x")
                    onClicked: root.log.clear()
                }
            }

            RowLayout {
                visible: root.view === "other"
                spacing: Theme.space.sm

                ComboBox {
                    id: fileCombo
                    Layout.preferredWidth: 260
                    model: root.otherLogs
                    textRole: "name"
                    // Keeps its selection in sync with otherLogs.currentFile
                    // rather than owning the selection itself - a deleted
                    // file, or the model refreshing after one appears,
                    // should not silently pick something else.
                    currentIndex: {
                        if (!root.otherLogs) return -1
                        for (var i = 0; i < count; i++) {
                            if (textAt(i) === root.otherLogs.currentFile) return i
                        }
                        return -1
                    }
                    onActivated: (index) => root.otherLogs.selectFile(textAt(index))
                }
                Button {
                    enabled: !!root.otherLogs && root.otherLogs.currentFile.length > 0
                    text: qsTr("Copy")
                    icon.source: Icons.url("copy")
                    onClicked: {
                        clipboardHelper.text = root.otherLogs.content
                        clipboardHelper.selectAll()
                        clipboardHelper.copy()
                    }
                }
                Button {
                    enabled: !!root.otherLogs
                    flat: true
                    text: qsTr("Open folder")
                    icon.source: Icons.url("folder")
                    onClicked: root.openFolderRequested(root.otherLogs.path)
                }
                IconButton {
                    enabled: !!root.otherLogs && root.otherLogs.currentFile.length > 0
                    iconName: "trash"
                    tip: qsTr("Delete this file")
                    onClicked: {
                        deleteFileConfirm.text = qsTr("Delete “%1”? It cannot be recovered from the launcher.").arg(root.otherLogs.currentFile)
                        deleteFileConfirm.open()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: lines
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                visible: root.view === "live"
                clip: true
                model: root.model
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}

                onCountChanged: if (follow.checked) positionViewAtEnd()
                onMovementStarted: follow.checked = false
                onAtYEndChanged: if (atYEnd && moving) follow.checked = true

                delegate: Text {
                    required property string line
                    required property int level
                    width: lines.width - Theme.space.md
                    text: line
                    wrapMode: Text.WrapAnywhere
                    color: root.levelColor(level)
                    font.family: Theme.font.mono
                    font.pixelSize: Theme.type.label.pixelSize - 1
                    textFormat: Text.PlainText
                }
            }

            EmptyState {
                anchors.centerIn: parent
                upperThird: true
                visible: root.view === "live" && (!root.model || lines.count === 0)
                title: qsTr("No log yet")
                body: qsTr("Start the instance and its output appears here, live.")
                MeshIcon { iconName: "terminal"; size: 40; color: Theme.palette.textTertiary }
            }

            ScrollView {
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                visible: root.view === "other" && !!root.otherLogs && root.otherLogs.currentFile.length > 0
                clip: true

                TextArea {
                    readOnly: true
                    text: root.otherLogs ? root.otherLogs.content : ""
                    wrapMode: TextArea.WrapAnywhere
                    selectByMouse: true
                    font.family: Theme.font.mono
                    font.pixelSize: Theme.type.label.pixelSize - 1
                    background: Item {}
                }
            }

            EmptyState {
                anchors.centerIn: parent
                upperThird: true
                visible: root.view === "other" && (!root.otherLogs || root.otherLogs.currentFile.length === 0)
                title: fileCombo.count > 0 ? qsTr("No file selected") : qsTr("No other logs")
                body: fileCombo.count > 0
                      ? qsTr("Pick a file above to view it.")
                      : qsTr("Log files this instance writes outside the live console, such as rotated logs and crash reports, show up here.")
                MeshIcon { iconName: "terminal"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    // QML has no clipboard API of its own; an invisible text edit does.
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    ConfirmDialog {
        id: deleteFileConfirm
        title: qsTr("Delete log file")
        confirmText: qsTr("Delete")
        onConfirmed: if (root.otherLogs) root.otherLogs.deleteCurrent()
    }
}
