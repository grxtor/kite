// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's single-player worlds, most useful facts first: name,
 * game mode, when it was last played. Deleting one asks first -- a world
 * is hours of someone's play and there is no undo.
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

                width: list.width - Theme.space.md
                height: 76

                HoverHandler { id: hover }

                // A faint duplicate a few pixels below the card reads as a
                // soft drop shadow without a real blur (none of the effect
                // modules are available at this Qt floor).
                Rectangle {
                    x: 0; y: 3
                    width: parent.width
                    height: parent.height
                    radius: Theme.radius.lg
                    color: Theme.palette.scrim
                    opacity: hover.hovered ? 0.16 : 0.08
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                }

                Rectangle {
                    id: card
                    width: parent.width
                    height: parent.height
                    y: hover.hovered ? -1 : 0
                    radius: Theme.radius.lg
                    color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                    border.width: 1
                    border.color: hover.hovered ? Theme.palette.borderStrong : Theme.palette.border

                    Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space.md
                        anchors.rightMargin: Theme.space.md
                        spacing: Theme.space.md

                        Rectangle {
                            Layout.preferredWidth: 52
                            Layout.preferredHeight: 52
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            Image {
                                id: worldIcon
                                anchors.fill: parent
                                anchors.margins: 2
                                source: Format.fileUrl(cell.iconFile)
                                sourceSize: Qt.size(96, 96)
                                smooth: false
                                visible: status === Image.Ready
                            }
                            MeshIcon {
                                anchors.centerIn: parent
                                visible: !worldIcon.visible
                                iconName: "globe"
                                color: Theme.palette.textTertiary
                            }
                        }

                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Theme.space.xs
                            Text {
                                width: parent.width
                                text: cell.name.length > 0 ? cell.name : cell.folder
                                elide: Text.ElideRight
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.bodyStrong.pixelSize
                                font.weight: Theme.type.bodyStrong.weight
                            }
                            Row {
                                spacing: Theme.space.xs
                                Tag {
                                    visible: text.length > 0
                                    text: String(cell.gameMode || "")
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
                            visible: hover.hovered && root.unlocked
                            iconName: "package"
                            tip: qsTr("Data packs")
                            onClicked: root.dataPacksRequested(cell.index)
                        }

                        IconButton {
                            visible: hover.hovered && root.unlocked
                            enabled: !root.busy
                            iconName: "more"
                            tip: qsTr("More")
                            onClicked: rowMenu.popup()

                            Menu {
                                id: rowMenu
                                MenuItem {
                                    text: qsTr("Rename…")
                                    icon.source: Icons.url("edit")
                                    onTriggered: {
                                        renamePrompt.row = cell.index
                                        renamePrompt.value = cell.name.length > 0 ? cell.name : cell.folder
                                        renamePrompt.open()
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Copy…")
                                    icon.source: Icons.url("copy")
                                    onTriggered: {
                                        copyPrompt.row = cell.index
                                        copyPrompt.value = qsTr("%1 (copy)").arg(cell.name.length > 0 ? cell.name : cell.folder)
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
                                        confirm.text = qsTr("Delete the world “%1”? It cannot be recovered from the launcher.").arg(cell.name.length > 0 ? cell.name : cell.folder)
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
