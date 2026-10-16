// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Extra folders MeshMC also looks in for instances (Settings > General).
 * The main instance folder stays where new instances are made; these only
 * add instances that already live elsewhere. Each row has a ⋯ menu to open
 * or remove it, and "Add folder…" picks the next one. Removing a folder only
 * stops MeshMC looking there: nothing is deleted.
 */
Column {
    id: root

    readonly property var folders: LauncherStore.folders
    readonly property var dirs: folders ? folders.additionalDirs : []

    signal openRequested(string path)

    // The folder a menu is about.
    property string target

    visible: !!folders
    spacing: Theme.space.sm

    Text {
        text: qsTr("More instance folders")
        color: Theme.palette.textPrimary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.title.pixelSize
        font.weight: Font.Bold
    }
    Text {
        width: parent.width
        text: qsTr("Instances in these folders show up in the library too. New instances are still made in the main folder.")
        wrapMode: Text.Wrap
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }

    Rectangle {
        width: parent.width
        height: rows.height
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: rows
            width: parent.width

            Text {
                visible: root.dirs.length === 0
                width: parent.width
                height: Theme.control.heightLg + Theme.space.xl
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.space.lg
                text: qsTr("No extra folders.")
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }

            Repeater {
                model: root.dirs

                delegate: Item {
                    id: row
                    required property string modelData
                    required property int index

                    width: rows.width
                    height: Theme.control.heightLg + Theme.space.xl

                    Rectangle {
                        visible: row.index > 0
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Theme.space.lg
                        anchors.rightMargin: Theme.space.lg
                        height: 1
                        color: Theme.palette.divider
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space.lg
                        anchors.rightMargin: Theme.space.sm
                        spacing: Theme.space.md

                        Text {
                            Layout.fillWidth: true
                            text: row.modelData
                            elide: Text.ElideMiddle
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.mono
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                        IconButton {
                            id: more
                            iconName: "more"
                            tip: qsTr("More")
                            onClicked: {
                                root.target = row.modelData
                                menu.popup(more, more.width - menu.width, more.height)
                            }
                        }
                    }
                }
            }
        }
    }

    Button {
        text: qsTr("Add folder…")
        icon.source: Icons.url("plus")
        onClicked: dialog.open()
    }

    Menu {
        id: menu
        MenuItem {
            text: qsTr("Open folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openRequested(root.target)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Remove from the list")
            icon.source: Icons.url("trash")
            onTriggered: root.folders.removeAdditionalDir(root.target)
        }
    }

    FolderDialog {
        id: dialog
        title: qsTr("Add an instance folder")
        onAccepted: root.folders.addAdditionalDir(selectedFolder)
    }
}
