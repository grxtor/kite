// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * The instance's single-player worlds. "Add world" unpacks a .zip (picked or
 * dropped) into the worlds folder. Changing a world while the game runs can
 * corrupt it, so each such action asks "Continue anyway?" instead of being
 * unavailable.
 */
Item {
    id: root

    // InstanceDetails: worlds (WorldList), worldsDir, deleteWorld, renameWorld, copyWorld,
    // resetWorldIcon.
    property var details: null
    readonly property bool unlocked: !!details && details.contentChangesAllowed
    readonly property int count: list.count
    signal openFolderRequested(string path)
    // Asks the instance page to switch to the Data packs tab at world `row`.
    signal dataPacksRequested(int row)

    // The TaskWatcher of a copy or install in progress; both run off the GUI thread.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    property string busyText: qsTr("Copying world…")
    property string failText: qsTr("The world could not be copied.")
    property string doneText: qsTr("World copied.")
    // Whether the MCEdit setting points at a usable MCEdit; rechecked when a row menu opens.
    property bool mceditReady: false
    property var pendingAction: null

    function guarded(action) {
        if (root.unlocked) {
            action()
            return
        }
        root.pendingAction = action
        runningConfirm.open()
    }

    // Installs the first Minecraft world of @p fileUrls (file:// URLs). Only one install runs at a
    // time (one progress line), so a drop of several adds the first and says so.
    function addWorlds(fileUrls) {
        if (!root.details || root.busy)
            return
        for (var i = 0; i < fileUrls.length; ++i) {
            var started = root.details.installWorld(String(fileUrls[i]))
            if (!started)
                continue
            root.busyText = qsTr("Adding world…")
            root.failText = qsTr("The world could not be added.")
            root.doneText = fileUrls.length > 1 ? qsTr("World added. Add the others one at a time.")
                                                : qsTr("World added.")
            root.watcher = started
            return
        }
        toast.show(qsTr("That is not a Minecraft world. Pick a .zip with a world in it."), "danger")
    }

    // WorldList reports a level.dat without a mode as "Undefined" or "Unknown (n)"; only real modes
    // are shown.
    readonly property var knownModes: [qsTranslate("GameType", "Survival"), qsTranslate("GameType", "Creative"),
                                       qsTranslate("GameType", "Adventure"), qsTranslate("GameType", "Spectator")]

    onDetailsChanged: root.watcher = null

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        TabToolbar {
            Layout.fillWidth: true

            IconButton {
                flat: false
                iconName: "refresh"
                tip: qsTr("Refresh")
                onClicked: if (root.details) root.details.refreshWorlds()
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.details ? root.details.worldsDir : "")
            }
            Button {
                enabled: !!root.details && !root.busy
                text: qsTr("Add world")
                icon.source: Icons.url("plus")
                onClicked: root.guarded(() => addDialog.open())
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.busy || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.md

            Copy {
                Layout.fillWidth: true
                text: root.watcher
                      ? (root.watcher.failed ? (root.watcher.error || root.failText)
                                             : (root.watcher.status || root.busyText))
                      : ""
                role: "label"
                tone: root.watcher && root.watcher.failed ? "danger" : "secondary"
                elide: Text.ElideRight
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
            model: root.details ? root.details.worlds : null
            ScrollBar.vertical: ScrollBar {}

            delegate: TabListRow {
                id: cell
                required property int index
                required property string name
                required property string folder
                required property var gameMode
                required property var lastPlayed
                required property var iconFile
                required property var dayCount
                required property var seed

                readonly property string displayName: cell.name.length > 0 ? cell.name : cell.folderName
                readonly property string folderName: String(cell.folder).split(/[\\/]/).pop()
                readonly property string mode: String(cell.gameMode || "")

                width: list.width
                title: cell.displayName
                meta: [root.knownModes.indexOf(cell.mode) >= 0 ? cell.mode : "",
                       cell.dayCount !== undefined && cell.dayCount !== null ? qsTr("Day %1").arg(cell.dayCount) : "",
                       Format.lastPlayed(cell.lastPlayed ? Number(cell.lastPlayed) : 0)]
                      .filter((part) => part.length > 0).join("  •  ")
                // A neutral globe stands in for a missing icon: a landscape would pass for a real
                // view of the world.
                thumb.source: Format.fileUrl(cell.iconFile)
                thumb.glyph: "globe"
                thumb.fill: true
                thumb.pixelArt: true
                onMenuRequested: if (!root.busy) rowMenu.popup()

                IconButton {
                    id: moreButton
                    enabled: !root.busy
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)

                    Menu {
                        id: rowMenu
                        onAboutToShow: root.mceditReady = !!root.details && root.details.mceditConfigured()
                        MenuEntry {
                            visible: !!root.details && root.details.isMinecraft
                            text: qsTr("Data packs")
                            icon.source: Icons.url("package")
                            onTriggered: root.dataPacksRequested(cell.index)
                        }
                        MenuItem {
                            text: qsTr("Copy seed")
                            icon.source: Icons.url("copy")
                            onTriggered: {
                                clipboard.copyText(String(cell.seed))
                                toast.show(qsTr("Seed copied."), "success")
                            }
                        }
                        MenuEntry {
                            visible: root.mceditReady
                            text: qsTr("Open in MCEdit")
                            icon.source: Icons.url("external-link")
                            onTriggered: root.guarded(() => {
                                var problem = root.details.openWorldInMCEdit(cell.index)
                                if (problem.length > 0)
                                    toast.show(problem, "danger")
                            })
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: qsTr("Rename…")
                            icon.source: Icons.url("edit")
                            onTriggered: root.guarded(() => {
                                renamePrompt.row = cell.index
                                renamePrompt.value = cell.displayName
                                renamePrompt.open()
                            })
                        }
                        MenuItem {
                            text: qsTr("Copy…")
                            icon.source: Icons.url("copy")
                            onTriggered: root.guarded(() => {
                                copyPrompt.row = cell.index
                                copyPrompt.value = qsTr("%1 (copy)").arg(cell.displayName)
                                copyPrompt.open()
                            })
                        }
                        MenuItem {
                            text: qsTr("Reset icon")
                            icon.source: Icons.url("image")
                            enabled: !!cell.iconFile
                            onTriggered: root.guarded(() => root.details.resetWorldIcon(cell.index))
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: qsTr("Delete…")
                            icon.source: Icons.url("trash")
                            onTriggered: root.guarded(() => {
                                confirm.row = cell.index
                                confirm.text = qsTr("Delete the world “%1”? It cannot be recovered from the launcher.").arg(cell.displayName)
                                confirm.open()
                            })
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

    ConfirmDialog {
        id: confirm
        property int row: -1
        title: qsTr("Delete world")
        confirmText: qsTr("Delete world")
        onConfirmed: if (root.details) root.details.deleteWorld(row)
    }

    PromptDialog {
        id: renamePrompt
        property int row: -1
        title: qsTr("Rename world")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (!root.details || root.details.renameWorld(row, text))
                close()
            else
                error = qsTr("That name cannot be used.")
        }
    }

    PromptDialog {
        id: copyPrompt
        property int row: -1
        title: qsTr("Copy world")
        confirmText: qsTr("Copy")
        onSubmitted: (text) => {
            var watcher = root.details ? root.details.copyWorld(row, text) : null
            if (watcher) {
                root.busyText = qsTr("Copying world…")
                root.failText = qsTr("The world could not be copied.")
                root.doneText = qsTr("World copied.")
                root.watcher = watcher
                close()
            } else {
                error = qsTr("That name cannot be used.")
            }
        }
    }

    ConfirmDialog {
        id: runningConfirm
        title: qsTr("Minecraft is running")
        text: qsTr("Changing a world while Minecraft is running can damage it. Continue anyway?")
        confirmText: qsTr("Continue anyway")
        onConfirmed: {
            var action = root.pendingAction
            root.pendingAction = null
            if (action)
                action()
        }
    }

    FileDialog {
        id: addDialog
        title: qsTr("Add world")
        nameFilters: [qsTr("World archives (*.zip)"), qsTr("All files (*)")]
        onAccepted: root.addWorlds([selectedFile])
    }

    Connections {
        target: root.watcher
        ignoreUnknownSignals: true
        function onFinished(ok) {
            if (ok)
                toast.show(root.doneText, "success")
        }
    }

    ClipboardText { id: clipboard }

    Toast { id: toast }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No worlds yet")
        body: qsTr("Worlds you create in single player show up here. You can also add one from a .zip.")
        MeshIcon { iconName: "globe"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }
}
