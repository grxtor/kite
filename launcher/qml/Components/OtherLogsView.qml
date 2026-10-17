// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Log files on disk: pick one, read it, search it, copy it, upload it,
 * delete it. Used for an instance's other logs (logs/*.log*, crash reports)
 * on the Log tab and for the launcher's own logs in Settings -- both are an
 * OtherLogsModel, so both look and work the same.
 *
 * The toolbar is one row, like the live log's: file, find, then the
 * actions, with the rarer ones (wrap lines, reload, delete) behind ⋯.
 *
 * Reading goes through LogLines like the live log does, so Find and Wrap
 * work the same on both. Selecting text with the mouse does not (it is a
 * list of lines, not one text); "Copy" takes the whole file.
 */
Item {
    id: root

    // OtherLogsModel (roles: name; currentFile, content, path, selectFile(),
    // reload(), deleteCurrent(), deleteAll()).
    property var logs: null
    // LogUploader: uploadFile(path), busy; uploaded(url) / failed(message).
    property var uploader: null
    // Picks the first (newest) file when nothing is picked yet.
    property bool selectFirst: false

    // Anything added inside this view's tag goes at the start of its toolbar
    // (the Log tab puts its log picker there, with `ownPicker` off so the
    // file picker below is not shown twice).
    default property alias leading: leadingRow.data
    property bool ownPicker: true

    signal openFolderRequested(string path)

    // The file picker shrinks before the buttons beside it get squeezed out
    // of a narrow column (the Settings page keeps it under 760 wide).
    readonly property int comboWidth: 220
    readonly property int comboMinWidth: 120
    // Below this width the Copy, Upload and Open folder buttons keep only
    // their icons (their label becomes the tooltip).
    readonly property int compactBelow: 700
    readonly property bool compact: root.width < root.compactBelow

    readonly property bool hasFile: !!logs && logs.currentFile.length > 0
    // Set by this view's own Upload button, so a second log view sharing the
    // uploader does not also copy and announce the link.
    property bool uploading: false

    // The file's text, one entry per line.
    readonly property var lines: {
        if (!logs || !logs.content)
            return []
        var all = logs.content.split("\n")
        if (all.length > 0 && all[all.length - 1] === "")
            all.pop()
        return all
    }

    property var matches: []
    property int current: -1

    function recomputeMatches() {
        var needle = findBar.text.toLowerCase()
        var found = []
        if (needle.length > 0) {
            var all = root.lines
            for (var i = 0; i < all.length; ++i) {
                if (all[i].toLowerCase().indexOf(needle) >= 0)
                    found.push(i)
            }
        }
        root.matches = found
        root.current = found.length > 0 ? 0 : -1
    }

    function step(delta) {
        if (root.matches.length === 0)
            return
        root.current = (root.current + delta + root.matches.length) % root.matches.length
    }

    function pickFirst() {
        if (root.selectFirst && root.logs && root.logs.currentFile.length === 0 && fileCombo.count > 0)
            root.logs.selectFile(fileCombo.textAt(0))
    }

    Component.onCompleted: pickFirst()
    onLogsChanged: pickFirst()

    onLinesChanged: findDelay.restart()

    Timer {
        id: findDelay
        interval: 150
        onTriggered: root.recomputeMatches()
    }

    Connections {
        target: root.logs
        ignoreUnknownSignals: true
        // The list of files was rebuilt (a file appeared or went away).
        function onModelReset() { root.pickFirst() }
    }

    Connections {
        target: root.uploader
        ignoreUnknownSignals: true
        function onUploaded(url) {
            if (!root.uploading)
                return
            root.uploading = false
            clipboard.copyText(url)
            toast.show(qsTr("Link copied to your clipboard."), "success", qsTr("Open"),
                       () => Qt.openUrlExternally(url))
        }
        function onFailed(message) {
            if (!root.uploading)
                return
            root.uploading = false
            toast.show(message, "danger")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            RowLayout {
                id: leadingRow
                visible: children.length > 0
                spacing: Theme.space.sm
            }

            ComboBox {
                id: fileCombo
                visible: root.ownPicker
                Layout.preferredWidth: root.comboWidth
                Layout.minimumWidth: root.comboMinWidth
                model: root.logs
                textRole: "name"
                // Keeps its selection in sync with logs.currentFile rather
                // than owning the selection itself -- a deleted file, or the
                // model refreshing after one appears, should not silently
                // pick something else.
                currentIndex: {
                    if (!root.logs) return -1
                    for (var i = 0; i < count; i++) {
                        if (textAt(i) === root.logs.currentFile) return i
                    }
                    return -1
                }
                onActivated: (index) => root.logs.selectFile(textAt(index))
                // Says what it is for before a file is picked, rather than
                // sitting there empty.
                displayText: currentIndex < 0 && count > 0 ? qsTr("Choose a log file") : currentText
            }

            LogFindBar {
                id: findBar
                Layout.fillWidth: true
                visible: root.hasFile
                matchCount: root.matches.length
                current: root.current
                onTextChanged: findDelay.restart()
                onNext: root.step(1)
                onPrevious: root.step(-1)
            }

            Item {
                Layout.fillWidth: true
                visible: !root.hasFile
            }

            Button {
                enabled: root.hasFile
                text: qsTr("Copy")
                icon.source: Icons.url("copy")
                display: root.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ToolTip.visible: root.compact && hovered
                ToolTip.text: text
                onClicked: {
                    clipboard.copyText(root.logs.content)
                    toast.show(qsTr("Log copied."), "success")
                }
            }
            Button {
                visible: !!root.uploader
                enabled: root.hasFile && !!root.uploader && !root.uploader.busy
                text: root.uploading ? qsTr("Uploading…") : qsTr("Upload")
                icon.source: Icons.url("arrow-up")
                display: root.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ToolTip.visible: root.compact && hovered
                ToolTip.text: text
                onClicked: {
                    root.uploading = true
                    root.uploader.uploadFile(root.logs.path + "/" + root.logs.currentFile)
                }
            }
            Button {
                enabled: !!root.logs
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                display: root.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ToolTip.visible: root.compact && hovered
                ToolTip.text: text
                onClicked: root.openFolderRequested(root.logs.path)
            }
            IconButton {
                id: moreButton
                enabled: !!root.logs && fileCombo.count > 0
                flat: false
                iconName: "more"
                tip: qsTr("More")
                onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height)

                Menu {
                    id: moreMenu
                    MenuItem {
                        text: qsTr("Wrap lines")
                        checkable: true
                        checked: findBar.wrap
                        onToggled: findBar.wrap = checked
                    }
                    MenuItem {
                        enabled: root.hasFile
                        text: qsTr("Reload")
                        icon.source: Icons.url("refresh")
                        onTriggered: root.logs.reload()
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("Delete this file…")
                        icon.source: Icons.url("trash")
                        enabled: root.hasFile
                        onTriggered: {
                            deleteFileConfirm.text = qsTr("Delete “%1”? It cannot be recovered from the launcher.").arg(root.logs.currentFile)
                            deleteFileConfirm.open()
                        }
                    }
                    MenuItem {
                        text: qsTr("Delete all…")
                        icon.source: Icons.url("trash")
                        onTriggered: {
                            deleteAllConfirm.text = fileCombo.count === 1
                                ? qsTr("Delete the log file? It cannot be recovered from the launcher.")
                                : qsTr("Delete all %1 log files? They cannot be recovered from the launcher.").arg(fileCombo.count)
                            deleteAllConfirm.open()
                        }
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

            LogLines {
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                visible: root.hasFile
                model: root.lines
                wrap: findBar.wrap
                matches: root.matches
                current: root.current
            }

            EmptyState {
                anchors.centerIn: parent
                upperThird: true
                visible: !root.hasFile
                title: fileCombo.count > 0 ? qsTr("No file selected") : qsTr("No logs")
                body: fileCombo.count > 0
                      ? qsTr("Pick a file above to view it.")
                      : qsTr("There are no log files here yet.")
                MeshIcon { iconName: "terminal"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    ClipboardText { id: clipboard }

    ConfirmDialog {
        id: deleteFileConfirm
        title: qsTr("Delete log file")
        confirmText: qsTr("Delete")
        onConfirmed: if (root.logs && !root.logs.deleteCurrent())
                         toast.show(qsTr("The file could not be deleted."), "danger")
    }

    ConfirmDialog {
        id: deleteAllConfirm
        title: qsTr("Delete logs")
        confirmText: qsTr("Delete all")
        onConfirmed: if (root.logs) {
            var failed = root.logs.deleteAll()
            if (failed.length > 0)
                toast.show(qsTr("Some files could not be deleted. They may be in use."), "danger")
        }
    }

    Toast { id: toast }
}
