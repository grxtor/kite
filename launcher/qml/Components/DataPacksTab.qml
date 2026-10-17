// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Data packs for one of this instance's worlds (saves/<world>/datapacks) --
 * the widget-free replacement for the modal dialog WorldListPage's "Data
 * packs" action used to open. The world picker defaults to the first world;
 * WorldsTab's own "Data packs" row action points this at a specific one via
 * `selectedWorldRow`.
 */
Item {
    id: root

    // InstanceDetails: worlds, worldDataPacks (WorldDataPacksController).
    property var details: null
    // Set from outside (WorldsTab's per-row action) to jump straight to a
    // world; -1 means "whatever the picker currently shows" (defaults to 0).
    property int selectedWorldRow: -1
    signal openFolderRequested(string path)

    readonly property var worldsModel: root.details ? root.details.worlds : null
    readonly property var controller: root.details ? root.details.worldDataPacks : null
    readonly property bool unlocked: !!root.controller && root.controller.unlocked
    readonly property int count: list.count
    readonly property string lockedReason: qsTr("The game is running. Data packs can be changed once it has closed.")
    readonly property int thumbSize: 36
    readonly property int pickerWidth: 220

    function openWorld(row) {
        if (!root.controller || row < 0)
            return
        picker.currentIndex = row
        root.controller.openForWorld(row)
    }

    onSelectedWorldRowChanged: if (selectedWorldRow >= 0) openWorld(selectedWorldRow)
    // Fires at creation too (see ManagedPackTab.qml's onControllerChanged
    // for why that still counts) and again whenever a different instance's
    // controller replaces this one.
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
                Text {
                    text: qsTr("World")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                },
                ComboBox {
                    id: picker
                    Layout.preferredWidth: root.pickerWidth
                    model: root.worldsModel
                    textRole: "name"
                    onActivated: (index) => root.openWorld(index)
                    // The world list may still be loading when the controller
                    // arrives; open the default world as soon as there is one,
                    // or the toolbar would stay disabled with nothing to act on.
                    onCountChanged: if (root.controller && !root.controller.ready) root.openDefault()
                }
            ]

            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                enabled: root.controller && root.controller.ready
                onClicked: root.openFolderRequested(root.controller ? root.controller.directory : "")
            }
            // Secondary: the play bar's Play is this page's primary action.
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
            // The same row rhythm as every list tab (see PackList).
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.controller ? root.controller.model : null
            // Overlays the rows' right edge rather than reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property var model
                readonly property string name: model.name
                readonly property var version: model.version
                readonly property bool itemEnabled: model.enabled

                width: list.width
                height: Theme.control.heightLg + Theme.space.md
                radius: Theme.radius.lg
                color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
                HoverHandler { id: hover }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: rowMenu.popup()
                }

                // Same row as Content's (PackList): what it is on the left,
                // its switch and ⋯ menu on the right.
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.sm
                    spacing: Theme.space.md

                    Rectangle {
                        Layout.preferredWidth: root.thumbSize
                        Layout.preferredHeight: root.thumbSize
                        radius: Theme.radius.sm
                        color: Theme.palette.surfaceSunken
                        opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                        MeshIcon { anchors.centerIn: parent; iconName: "package"; size: Theme.icon.sm; color: Theme.palette.textTertiary }
                    }

                    Column {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1
                        Text {
                            width: parent.width
                            text: row.name
                            elide: Text.ElideRight
                            color: row.itemEnabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.Medium
                        }
                        Text {
                            width: parent.width
                            visible: text.length > 0
                            text: row.version ? String(row.version) : ""
                            elide: Text.ElideRight
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }

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
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: picker.count > 0 && list.count === 0
        title: qsTr("No data packs")
        body: qsTr("Data packs change this world's recipes, loot and more. Add a .zip to get started.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Add data pack")
        nameFilters: [qsTr("Data pack files (*.zip)"), qsTr("All files (*)")]
        onAccepted: if (root.controller) root.controller.install(selectedFile.toString())
    }
}
