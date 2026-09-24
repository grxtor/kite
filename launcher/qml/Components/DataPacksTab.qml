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

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Text {
                text: qsTr("World:")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            ComboBox {
                id: picker
                Layout.preferredWidth: 220
                model: root.worldsModel
                textRole: "name"
                onActivated: (index) => root.openWorld(index)
            }
            Text {
                Layout.fillWidth: true
                text: !root.unlocked && root.controller ? qsTr("The game is running; data packs can be changed once it has closed.") : ""
                color: Theme.palette.warning
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                enabled: root.controller && root.controller.ready
                onClicked: root.openFolderRequested(root.controller ? root.controller.directory : "")
            }
            Button {
                enabled: root.unlocked && root.controller && root.controller.ready
                text: qsTr("Add from file…")
                icon.source: Icons.url("plus")
                onClicked: fileDialog.open()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.controller ? root.controller.model : null
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property var model
                readonly property string name: model.name
                readonly property var version: model.version
                readonly property bool itemEnabled: model.enabled

                width: list.width - Theme.space.md
                height: Theme.control.heightLg + Theme.space.md
                radius: Theme.radius.md
                color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.sm
                    anchors.rightMargin: Theme.space.sm
                    spacing: Theme.space.md

                    Switch {
                        checked: row.itemEnabled
                        enabled: root.unlocked
                        Accessible.name: qsTr("Enable %1").arg(row.name)
                        onToggled: {
                            root.controller.setEnabled(row.index, checked)
                            checked = Qt.binding(() => row.itemEnabled)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: Theme.radius.md
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

                    IconButton {
                        visible: hover.hovered && root.unlocked
                        iconName: "trash"
                        tip: qsTr("Remove")
                        onClicked: {
                            confirm.row = row.index
                            confirm.text = qsTr("Remove “%1” from this world? The file is deleted.").arg(row.name)
                            confirm.open()
                        }
                    }
                }
            }
        }
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
        body: qsTr("Add one from a file to enable it for this world.")
        actionText: qsTr("Add from file…")
        onActionTriggered: fileDialog.open()
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Add data pack")
        nameFilters: [qsTr("Data pack files (*.zip)"), qsTr("All files (*)")]
        onAccepted: if (root.controller) root.controller.install(selectedFile.toString())
    }
}
