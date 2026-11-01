// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Java runtimes Kite downloaded itself, as a card of rows with a ⋯ menu to use
 * or remove one. Removing deletes the runtime's whole folder, so it asks first.
 */
BoxSurface {
    id: root

    readonly property var java: JavaStore.controller
    readonly property var runtimes: java ? java.managed : []
    property string currentPath
    signal useRequested(string path)
    signal failed(string message)

    property var target: null

    implicitHeight: list.height

    Column {
        id: list
        width: parent.width

        Copy {
            visible: root.runtimes.length === 0
            width: parent.width
            height: Theme.control.heightLg + Theme.space.xl
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.space.lg
            rightPadding: Theme.space.lg
            text: qsTr("Nothing downloaded yet.")
            tone: "tertiary"
        }

        Repeater {
            model: root.runtimes

            delegate: JavaInstallRow {
                id: row
                required property var modelData
                required property int index

                width: list.width
                interactive: false
                showDivider: row.index > 0
                title: row.modelData.name
                location: row.modelData.javaPath
                vendor: row.modelData.vendor
                inUse: root.currentPath === row.modelData.javaPath

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
