// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The Java runtimes MeshMC downloaded itself, as a card of rows: which one,
 * where it lives, and a ⋯ menu to use or remove it. Removing deletes the
 * runtime's whole folder, so it asks first. Empty when nothing has been
 * downloaded; the section above offers Download Java then.
 */
Rectangle {
    id: root

    readonly property var java: JavaStore.controller
    readonly property var runtimes: java ? java.managed : []
    // The JavaPath in use, marked in the list.
    property string currentPath
    signal useRequested(string path)
    signal failed(string message)

    // The runtime a menu or confirmation is about.
    property var target: null

    implicitHeight: list.height
    radius: Theme.radius.lg
    color: Theme.palette.surface
    border.width: 1
    border.color: Theme.palette.border

    Column {
        id: list
        width: parent.width

        Text {
            visible: root.runtimes.length === 0
            width: parent.width
            height: Theme.control.heightLg + Theme.space.xl
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.space.lg
            rightPadding: Theme.space.lg
            text: qsTr("Nothing downloaded yet.")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
        }

        Repeater {
            model: root.runtimes

            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool inUse: root.currentPath === modelData.javaPath

                width: list.width
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

                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: row.modelData.name
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.Medium
                        }
                        Text {
                            width: parent.width
                            text: row.modelData.javaPath
                            elide: Text.ElideMiddle
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.mono
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                    Tag { text: row.modelData.vendor }
                    StatusBadge {
                        visible: row.inUse
                        tone: "success"
                        text: qsTr("In use")
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

    Menu {
        id: menu
        MenuItem {
            text: qsTr("Use this Java")
            icon.source: Icons.url("check")
            enabled: !!root.target && root.target.javaPath !== root.currentPath
            onTriggered: root.useRequested(root.target.javaPath)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Remove…")
            icon.source: Icons.url("trash")
            onTriggered: confirm.open()
        }
    }

    ConfirmDialog {
        id: confirm
        title: qsTr("Remove this Java?")
        text: root.target
              ? qsTr("%1 will be deleted from this computer. Instances using it fall back to automatic Java.").arg(root.target.name)
              : ""
        confirmText: qsTr("Remove")
        onConfirmed: {
            var error = root.java.removeManaged(root.target.root)
            if (error.length > 0)
                root.failed(error)
        }
    }
}
