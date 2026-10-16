// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's single-player worlds, most useful facts first: name,
 * game mode, when it was last played. Every row's actions sit behind its
 * ⋯ menu (also on right-click). Deleting one asks first -- a world is hours
 * of someone's play and there is no undo.
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

    // The TaskWatcher of a copy in progress, if any - copyWorld() runs off
    // the GUI thread (a world can run into gigabytes), see InstanceDetails.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running

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

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: list.count > 0 ? qsTr("%1 worlds").arg(list.count) : ""
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.details ? root.details.worldsDir : "")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.busy || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.md

            Text {
                Layout.fillWidth: true
                text: root.watcher
                      ? (root.watcher.failed ? (root.watcher.error || qsTr("Copy failed."))
                                             : (root.watcher.status || qsTr("Copying world…")))
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

                readonly property string displayName: cell.name.length > 0 ? cell.name : cell.folderName
                // The folder's own name: the seed Home's world tiles and the
                // overview use too, so a world without an icon shows the
                // same scene everywhere. `folder` is the full path.
                readonly property string folderName: String(cell.folder).split(/[\\/]/).pop()
                readonly property string mode: String(cell.gameMode || "")

                width: list.width
                height: root.rowHeight

                HoverHandler { id: hover }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: if (root.unlocked && !root.busy) rowMenu.popup()
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
                            // No icon: one of PixelArt's rendered scenes,
                            // picked by the folder name (see folderName).
                            Image {
                                anchors.fill: parent
                                visible: !worldIcon.visible
                                source: worldIcon.visible ? "" : PixelArt.worldUrlFor(cell.folderName)
                                sourceSize: Qt.size(width * 2, height * 2)
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                mipmap: true
                                asynchronous: true
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
                            enabled: root.unlocked && !root.busy
                            iconName: "more"
                            tip: qsTr("More")
                            onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)

                            Menu {
                                id: rowMenu
                                MenuItem {
                                    text: qsTr("Data packs")
                                    icon.source: Icons.url("package")
                                    onTriggered: root.dataPacksRequested(cell.index)
                                }
                                MenuSeparator {}
                                MenuItem {
                                    text: qsTr("Rename…")
                                    icon.source: Icons.url("edit")
                                    onTriggered: {
                                        renamePrompt.row = cell.index
                                        renamePrompt.value = cell.displayName
                                        renamePrompt.open()
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Copy…")
                                    icon.source: Icons.url("copy")
                                    onTriggered: {
                                        copyPrompt.row = cell.index
                                        copyPrompt.value = qsTr("%1 (copy)").arg(cell.displayName)
                                        copyPrompt.open()
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Reset icon")
                                    icon.source: Icons.url("image")
                                    enabled: !!cell.iconFile
                                    onTriggered: if (root.details) root.details.resetWorldIcon(cell.index)
                                }
                                MenuSeparator {}
                                MenuItem {
                                    text: qsTr("Delete…")
                                    icon.source: Icons.url("trash")
                                    onTriggered: {
                                        confirm.row = cell.index
                                        confirm.text = qsTr("Delete the world “%1”? It cannot be recovered from the launcher.").arg(cell.displayName)
                                        confirm.open()
                                    }
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
                root.watcher = watcher
                close()
            } else {
                error = qsTr("That name cannot be used.")
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No worlds yet")
        body: qsTr("Worlds you create in single player show up here.")
        MeshIcon { iconName: "globe"; size: 40; color: Theme.palette.textTertiary }
    }
}
