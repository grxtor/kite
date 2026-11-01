// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Kite.Theme

/*
 * Extra folders Kite also looks in for instances (Settings > General); the main instance folder stays where new
 * instances are made. Each row has a ⋯ menu to open or remove it, and "Add folder…" picks the next. Removing a folder
 * only stops Kite looking there: nothing is deleted.
 */
Column {
    id: root

    readonly property var folders: LauncherStore.folders
    readonly property var dirs: folders ? folders.additionalDirs : []

    signal openRequested(string path)

    property string target

    visible: !!folders
    spacing: Theme.space.sm

    GroupHeading {
        width: parent.width
        title: qsTr("More instance folders")
        description: qsTr("Instances in these folders show up in the library too. New instances are still made in the main folder.")
    }

    BoxSurface {
        width: parent.width
        height: rows.height

        Column {
            id: rows
            width: parent.width

            Copy {
                visible: root.dirs.length === 0
                width: parent.width
                height: Theme.control.heightLg + Theme.space.xl
                verticalAlignment: Text.AlignVCenter
                leftPadding: Theme.space.lg
                text: qsTr("No extra folders.")
                tone: "tertiary"
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
