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
 * for LogPage and OtherLogsPage, folded into one tab: both are "read a log",
 * just live vs. on disk. Which one is a single drop-down (LogSourcePicker,
 * "Live game log" first). The on-disk side is OtherLogsView, which the
 * launcher's own logs in Settings use too.
 *
 * "Live" follows the newest line only while "Follow" is on -- scrolling up
 * to read something turns it off, instead of the view yanking the reader
 * back down on every new line.
 *
 * Find marks every line that contains the text and steps through them; as
 * the log grows, the matches follow it. Upload sends the log to paste.ee
 * (the key set in Settings, Log upload), copies the link and says so. The
 * whole toolbar is one row: log, find, then the actions; the rarer ones
 * (wrap lines, clear) sit behind ⋯.
 */
Item {
    id: root

    // InstanceLogBridge: model (LogModel: line, level), hasLog, clear(),
    // text(), findLines().
    property var log: null
    // OtherLogsModel (roles: name), or null - see InstanceDetails.otherLogs.
    property var otherLogs: null
    // InstanceDetails, for the log uploader.
    property var details: null
    readonly property var uploader: details ? details.logUploader : null
    readonly property var model: log ? log.model : null

    signal openFolderRequested(string path)

    // "live" or "other".
    property string view: "live"

    // Live Find: row numbers of the lines that contain the text.
    property var matches: []
    property int current: -1
    // Set by the live Upload button, so the Other logs view (which shares the
    // uploader) does not also announce the link.
    property bool uploading: false

    // The log picker shrinks, down to this, before the row pushes past its
    // column; the Copy and Upload buttons keep only their icons on a narrow
    // tab (their label becomes the tooltip).
    readonly property int pickerWidth: 200
    readonly property int pickerMinWidth: 120
    readonly property int compactBelow: 700
    readonly property bool compact: root.width < root.compactBelow
    // Whether there are log files on disk to pick instead of the live log.
    readonly property bool hasFiles: !!root.otherLogs && livePicker.files.length > 0

    // Shows the log file @p name.
    function showFile(name) {
        if (!root.otherLogs)
            return
        root.otherLogs.selectFile(name)
        root.view = "other"
    }

    readonly property int findDelayMs: 150
    // While the log grows, matches are refreshed this often at most.
    readonly property int growDelayMs: 500

    // For review snapshots (Main.qml's "review=<name>" dev route, through
    // InstancePage): the Other logs view, or the find bar with a query.
    function review(name) {
        if (name === "otherlogs")
            root.view = "other"
        else if (name === "logfind")
            findBar.text = "Loading"
    }

    function recomputeMatches() {
        var found = []
        if (root.log && findBar.text.length > 0 && typeof root.log.findLines === "function")
            found = root.log.findLines(findBar.text)
        root.matches = found
        root.current = found.length > 0 ? 0 : -1
    }

    function step(delta) {
        if (root.matches.length === 0)
            return
        follow.checked = false
        root.current = (root.current + delta + root.matches.length) % root.matches.length
    }

    onModelChanged: findDelay.restart()

    Timer {
        id: findDelay
        interval: root.findDelayMs
        onTriggered: root.recomputeMatches()
    }

    // New lines can add matches; refresh them without searching per line.
    Timer {
        id: growDelay
        interval: root.growDelayMs
        onTriggered: {
            if (findBar.text.length === 0 || !root.log)
                return
            var keep = root.current
            var found = root.log.findLines(findBar.text)
            root.matches = found
            root.current = found.length === 0 ? -1 : Math.min(Math.max(keep, 0), found.length - 1)
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.visible && root.view === "live"
        onActivated: findBar.focusField()
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
                matchCount: root.matches.length
                current: root.current
                onTextChanged: findDelay.restart()
                onNext: root.step(1)
                onPrevious: root.step(-1)
            }

            Switch {
                id: follow
                checked: true
                text: qsTr("Follow")
            }
            Button {
                enabled: !!root.model
                text: qsTr("Copy all")
                icon.source: Icons.url("copy")
                display: root.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ToolTip.visible: root.compact && hovered
                ToolTip.text: text
                onClicked: {
                    clipboard.copyText(root.log.text())
                    toast.show(qsTr("Log copied."), "success")
                }
            }
            Button {
                visible: !!root.uploader
                enabled: !!root.model && !!root.uploader && !root.uploader.busy
                text: root.uploading ? qsTr("Uploading…") : qsTr("Upload")
                icon.source: Icons.url("arrow-up")
                display: root.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                ToolTip.visible: root.compact && hovered
                ToolTip.text: text
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
                            root.matches = []
                            root.current = -1
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
                id: lines
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                model: root.model
                wrap: findBar.wrap
                follow: follow.checked
                matches: root.matches
                current: root.current
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
                MeshIcon { iconName: "terminal"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    OtherLogsView {
        id: otherView
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
