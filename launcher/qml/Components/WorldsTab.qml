// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * The instance's single-player worlds, most useful facts first: name,
 * game mode, when it was last played. Every row's actions sit behind its
 * ⋯ menu (also on right-click). Deleting one asks first -- a world is hours
 * of someone's play and there is no undo.
 *
 * "Add world" unpacks a .zip (picked, or dropped onto the tab) into the
 * worlds folder. Changing a world while the game is running can corrupt it,
 * so every action that changes one asks "Continue anyway?" first instead of
 * being unavailable -- the same warning the widget version gave.
 */
Item {
    id: root

    // InstanceDetails: worlds (WorldList: folder, seed, name, gameMode,
    // lastPlayed, iconFile, dayCount), worldsDir, deleteWorld, renameWorld,
    // copyWorld, resetWorldIcon.
    property var details: null
    readonly property bool unlocked: !!details && details.contentChangesAllowed
    readonly property int count: list.count
    signal openFolderRequested(string path)
    // Asks the instance page to switch to the Data packs tab pointed at
    // world `row` - see InstancePage.qml.
    signal dataPacksRequested(int row)

    // The TaskWatcher of a copy or an install in progress, if any - both run
    // off the GUI thread (a world can run into gigabytes), see InstanceDetails.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    // What the watcher is doing, for the progress line and the toasts.
    property string busyText: qsTr("Copying world…")
    property string failText: qsTr("The world could not be copied.")
    property string doneText: qsTr("World copied.")
    // Whether the MCEdit setting points at a usable MCEdit; checked each time
    // a row menu opens, since it is a launcher setting that can change.
    property bool mceditReady: false
    // The action waiting on the "game is running" question.
    property var pendingAction: null

    // Runs @p action now, or after "Continue anyway?" while the game runs.
    function guarded(action) {
        if (root.unlocked) {
            action()
            return
        }
        root.pendingAction = action
        runningConfirm.open()
    }

    // Starts installing the first of @p fileUrls (an array of file:// URLs)
    // that is a Minecraft world. Only one install runs at a time -- they
    // would fight over the one progress line -- so a drop of several adds
    // the first and says so.
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

    readonly property int rowHeight: 76
    readonly property int thumbSize: 52
    // The game modes worth a tag. WorldList reports a level.dat without one
    // as "Undefined" (or "Unknown (n)"), which says nothing to a player, so
    // anything else is simply left out.
    readonly property var knownModes: [qsTranslate("GameType", "Survival"), qsTranslate("GameType", "Creative"),
                                       qsTranslate("GameType", "Adventure"), qsTranslate("GameType", "Spectator")]

    // A different instance's details: whatever this tab was doing belonged
    // to the previous one.
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
            // Secondary: the play bar's Play is this page's primary action.
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

            Text {
                Layout.fillWidth: true
                text: root.watcher
                      ? (root.watcher.failed ? (root.watcher.error || root.failText)
                                             : (root.watcher.status || root.busyText))
                      : ""
                color: root.watcher && root.watcher.failed ? Theme.palette.danger : Theme.palette.textSecondary
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
            model: root.details ? root.details.worlds : null
            // Overlays the rows' right edge rather than reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
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
                // The folder's own name, the title of a world without a
                // level name. `folder` is the full path.
                readonly property string folderName: String(cell.folder).split(/[\\/]/).pop()
                readonly property string mode: String(cell.gameMode || "")

                width: list.width
                height: root.rowHeight

                HoverHandler { id: hover }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: if (!root.busy) rowMenu.popup()
                }

                Rectangle {
                    id: card
                    anchors.fill: parent
                    radius: Theme.radius.lg
                    color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                    border.width: 1
                    border.color: Theme.palette.border

                    Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space.md
                        anchors.rightMargin: Theme.space.sm
                        spacing: Theme.space.md

                        Rectangle {
                            id: thumb
                            Layout.preferredWidth: root.thumbSize
                            Layout.preferredHeight: root.thumbSize
                            radius: Theme.radius.sm
                            color: Theme.palette.surfaceSunken
                            clip: true

                            // The world's own icon.png, kept crisp like the
                            // pixel picture it is.
                            Image {
                                id: worldIcon
                                anchors.fill: parent
                                source: Format.fileUrl(cell.iconFile)
                                sourceSize: Qt.size(width * 2, height * 2)
                                fillMode: Image.PreserveAspectCrop
                                smooth: false
                                asynchronous: true
                                visible: status === Image.Ready
                            }
                            // No icon.png: a neutral globe. A landscape
                            // here would pass for a real view of the world,
                            // and two worlds would share one.
                            MeshIcon {
                                anchors.centerIn: parent
                                visible: !worldIcon.visible
                                iconName: "globe"
                                size: Theme.icon.md
                                color: Theme.palette.textTertiary
                            }
                            CornerMatte {
                                cornerRadius: thumb.radius
                                matte: card.color
                            }
                        }

                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space.xs
                            Text {
                                width: parent.width
                                text: cell.displayName
                                elide: Text.ElideRight
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.bodyStrong.pixelSize
                                font.weight: Theme.type.bodyStrong.weight
                            }
                            Row {
                                spacing: Theme.space.xs
                                Tag {
                                    visible: root.knownModes.indexOf(cell.mode) >= 0
                                    text: cell.mode
                                }
                                Tag {
                                    visible: cell.dayCount !== undefined && cell.dayCount !== null
                                    text: qsTr("Day %1").arg(cell.dayCount)
                                }
                                Tag {
                                    iconName: "clock"
                                    text: Format.lastPlayed(cell.lastPlayed ? Number(cell.lastPlayed) : 0)
                                }
                            }
                        }

                        IconButton {
                            id: moreButton
                            enabled: !root.busy
                            iconName: "more"
                            tip: qsTr("More")
                            onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)

                            Menu {
                                id: rowMenu
                                onAboutToShow: root.mceditReady = !!root.details && root.details.mceditConfigured()
                                MenuItem {
                                    // An old-format instance has no Data packs tab.
                                    visible: !!root.details && root.details.isMinecraft
                                    height: visible ? implicitHeight : 0
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
                                MenuItem {
                                    visible: root.mceditReady
                                    height: visible ? implicitHeight : 0
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
        }
    }

    // Rows fade out at the fold instead of being sliced by the play bar.
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

    // Worlds dropped from the file manager. Only the first is added; the
    // rest would fight over the one progress line.
    DropArea {
        id: drop
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (event) => {
            if (event.hasUrls) {
                // The event is gone once this handler returns, but the
                // question may still be open.
                var urls = event.urls
                root.guarded(() => root.addWorlds(urls))
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: drop.containsDrag
            radius: Theme.radius.lg
            color: Theme.palette.accentSubtle
            border.width: 1
            border.color: Theme.palette.accent

            Text {
                anchors.centerIn: parent
                text: qsTr("Drop a world here to add it")
                color: Theme.palette.accentText
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }
        }
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
        MeshIcon { iconName: "globe"; size: 40; color: Theme.palette.textTertiary }
    }
}
