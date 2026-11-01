// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * Data packs for one of this instance's worlds (saves/<world>/datapacks). The picker defaults to the first world;
 * WorldsTab's row action points it at one via `selectedWorldRow`.
 */
Item {
    id: root

    property var details: null
    // Set by WorldsTab's row action to jump to a world; -1 means whatever the picker shows (0 by default).
    property int selectedWorldRow: -1
    signal openFolderRequested(string path)

    readonly property var worldsModel: root.details ? root.details.worlds : null
    readonly property var controller: root.details ? root.details.worldDataPacks : null
    readonly property bool unlocked: !!root.controller && root.controller.unlocked
    readonly property int count: list.count
    readonly property string lockedReason: qsTr("The game is running. Data packs can be changed once it has closed.")
    readonly property int pickerWidth: 220

    function openWorld(row) {
        if (!root.controller || row < 0)
            return
        picker.currentIndex = row
        root.controller.openForWorld(row)
    }

    onSelectedWorldRowChanged: if (selectedWorldRow >= 0) openWorld(selectedWorldRow)
    // Fires at creation too, and whenever a different instance's controller replaces this one.
    onControllerChanged: openDefault()
    function openDefault() {
        if (root.selectedWorldRow >= 0)
            openWorld(root.selectedWorldRow)
        else if (root.worldsModel && picker.count > 0)
            openWorld(0)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: picker.count > 0

        TabToolbar {
            Layout.fillWidth: true

            leading: [
                Copy {
                    text: qsTr("World")
                    role: "label"
                    tone: "secondary"
                },
                ComboBox {
                    id: picker
                    Layout.preferredWidth: root.pickerWidth
                    model: root.worldsModel
                    textRole: "name"
                    onActivated: (index) => root.openWorld(index)
                    // The world list may still be loading when the controller arrives; open the default world once
                    // there is one.
                    onCountChanged: if (root.controller && !root.controller.ready) root.openDefault()
                },
                Item { Layout.fillWidth: true }
            ]

            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                enabled: root.controller && root.controller.ready
                onClicked: root.openFolderRequested(root.controller ? root.controller.directory : "")
            }
            HoverReason {
                reason: root.lockedReason
                Button {
                    enabled: root.unlocked && root.controller && root.controller.ready
                    text: qsTr("Add from file…")
                    icon.source: Icons.url("plus")
                    onClicked: fileDialog.open()
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !root.unlocked && !!root.controller
            text: root.lockedReason
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.controller ? root.controller.model : null
            ScrollBar.vertical: ScrollBar {}

            delegate: TabListRow {
                id: row
                required property int index
                required property var model
                readonly property string name: model.name
                readonly property var version: model.version
                readonly property bool itemEnabled: model.enabled

                width: list.width
                title: row.name
                meta: row.version ? String(row.version) : ""
                dimmed: !row.itemEnabled
                thumb.glyph: "package"
                onMenuRequested: rowMenu.popup()

                Switch {
                    checked: row.itemEnabled
                    enabled: root.unlocked
                    Accessible.name: qsTr("Enable %1").arg(row.name)
                    onToggled: {
                        root.controller.setEnabled(row.index, checked)
                        checked = Qt.binding(() => row.itemEnabled)
                    }
                }

                IconButton {
                    id: moreButton
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)
                }

                Menu {
                    id: rowMenu
                    MenuItem {
                        enabled: root.unlocked
                        text: row.itemEnabled ? qsTr("Disable") : qsTr("Enable")
                        onTriggered: root.controller.setEnabled(row.index, !row.itemEnabled)
                    }
                    MenuSeparator {}
                    MenuItem {
                        enabled: root.unlocked
                        text: qsTr("Remove…")
                        icon.source: Icons.url("trash")
                        onTriggered: {
                            confirm.row = row.index
                            confirm.text = qsTr("Remove “%1” from this world? The file is deleted.").arg(row.name)
                            confirm.open()
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
        title: qsTr("Remove data pack")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.controller) root.controller.remove(row)
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: picker.count === 0
        title: qsTr("No worlds yet")
        body: qsTr("Data packs live inside a world's own folder, so create a world first.")
        MeshIcon { iconName: "package"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: picker.count > 0 && list.count === 0
        title: qsTr("No data packs")
        body: qsTr("Data packs change this world's recipes, loot and more. Add a .zip to get started.")
        MeshIcon { iconName: "package"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Add data pack")
        nameFilters: [qsTr("Data pack files (*.zip)"), qsTr("All files (*)")]
        onAccepted: if (root.controller) root.controller.install(selectedFile.toString())
    }
}
