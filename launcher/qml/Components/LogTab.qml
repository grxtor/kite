// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The game's output, live, plus the instance's other log files (logs/*.log*,
 * crash-reports/*.txt) behind one drop-down (LogSourcePicker). "Live" follows
 * the newest line only while "Follow" is on; scrolling up turns it off. Upload
 * sends the log to paste.ee (key set in Settings, Log upload) and copies the
 * link.
 */
Item {
    id: root

    // InstanceLogBridge: model (LogModel: line, level), hasLog, clear(),
    // text(), findLines().
    property var log: null
    // OtherLogsModel (roles: name), or null.
    property var otherLogs: null
    property var details: null
    readonly property var uploader: details ? details.logUploader : null
    readonly property var model: log ? log.model : null

    signal openFolderRequested(string path)

    property string view: "live"

    // Set by the live Upload button, so the Other logs view (which shares the
    // uploader) does not also announce the link.
    property alias uploading: linkToast.pending

    // The log picker shrinks down to this before the row pushes past its column; Copy and Upload
    // keep only their icons on a narrow tab.
    readonly property int pickerWidth: 200
    readonly property int pickerMinWidth: 120
    readonly property int compactBelow: 700
    readonly property bool compact: root.width < root.compactBelow
    readonly property bool hasFiles: !!root.otherLogs && livePicker.files.length > 0

    function showFile(name) {
        if (!root.otherLogs)
            return
        root.otherLogs.selectFile(name)
        root.view = "other"
    }

    readonly property int growDelayMs: 500

    // For the "review=<name>" dev route: the Other logs view, or the find bar with a query.
    function review(name) {
        if (name === "otherlogs")
            root.view = "other"
        else if (name === "logfind")
            findBar.text = "Loading"
    }

    onModelChanged: finder.schedule()

    LogMatches {
        id: finder
        onSearch: setMatches(root.log && findBar.text.length > 0 && typeof root.log.findLines === "function"
                             ? root.log.findLines(findBar.text) : [])
        onStepped: follow.checked = false
    }

    Timer {
        id: growDelay
        interval: root.growDelayMs
        onTriggered: {
            if (findBar.text.length === 0 || !root.log)
                return
            var keep = finder.current
            var found = root.log.findLines(findBar.text)
            finder.matches = found
            finder.current = found.length === 0 ? -1 : Math.min(Math.max(keep, 0), found.length - 1)
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.visible && root.view === "live"
        onActivated: findBar.focusField()
    }

    UploadLinkToast {
        id: linkToast
        uploader: root.uploader
        clipboardItem: clipboard
        toastItem: toast
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: root.view === "live"

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            LogSourcePicker {
                id: livePicker
                visible: root.hasFiles
                Layout.preferredWidth: root.pickerWidth
                Layout.minimumWidth: root.pickerMinWidth
                logs: root.otherLogs
                live: true
                onFileChosen: (name) => root.showFile(name)
            }

            LogFindBar {
                id: findBar
                Layout.fillWidth: true
                searchEnabled: !!root.model
                matchCount: finder.matches.length
                current: finder.current
                onTextChanged: finder.schedule()
                onNext: finder.step(1)
                onPrevious: finder.step(-1)
            }

            Switch {
                id: follow
                checked: true
                text: qsTr("Follow")
            }
            CompactButton {
                compact: root.compact
                enabled: !!root.model
                text: qsTr("Copy all")
                icon.source: Icons.url("copy")
                onClicked: {
                    clipboard.copyText(root.log.text())
                    toast.show(qsTr("Log copied."), "success")
                }
            }
            CompactButton {
                compact: root.compact
                visible: !!root.uploader
                enabled: !!root.model && !!root.uploader && !root.uploader.busy
                text: root.uploading ? qsTr("Uploading…") : qsTr("Upload")
                icon.source: Icons.url("arrow-up")
                onClicked: {
                    root.uploading = true
                    root.uploader.uploadText(root.log.text())
                }
            }
            IconButton {
                id: moreButton
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
                    MenuSeparator {}
                    MenuItem {
                        enabled: !!root.model
                        text: qsTr("Clear log")
                        icon.source: Icons.url("x")
                        onTriggered: {
                            root.log.clear()
                            finder.clear()
                        }
                    }
                }
            }
        }

        BoxSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sunken: true

            LogLines {
                id: lines
                anchors.fill: parent
                anchors.margins: Theme.space.md
                model: root.model
                wrap: findBar.wrap
                follow: follow.checked
                matches: finder.matches
                current: finder.current
                onScrolledAway: follow.checked = false
                onReachedEnd: follow.checked = true
                onCountChanged: if (findBar.text.length > 0) growDelay.restart()
            }

            EmptyState {
                anchors.centerIn: parent
                upperThird: true
                visible: !root.model || lines.count === 0
                title: qsTr("No log yet")
                body: qsTr("Play this instance to see the game's output here as it runs.")
                MeshIcon { iconName: "terminal"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
            }
        }
    }

    OtherLogsView {
        anchors.fill: parent
        visible: root.view === "other"
        logs: root.otherLogs
        uploader: root.uploader
        ownPicker: false
        onOpenFolderRequested: (path) => root.openFolderRequested(path)

        LogSourcePicker {
            Layout.preferredWidth: root.pickerWidth
            Layout.minimumWidth: root.pickerMinWidth
            logs: root.otherLogs
            live: false
            onLiveChosen: root.view = "live"
            onFileChosen: (name) => root.showFile(name)
        }
    }

    ClipboardText { id: clipboard }

    Toast { id: toast }
}
