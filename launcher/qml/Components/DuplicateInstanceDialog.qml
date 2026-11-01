// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Duplicate…": a copy of one instance under a new name, with the choices of its worlds, play time and icon. Set
 * targetId, targetGroup, sourceName and iconKey, then open(). The copy (LibraryController.duplicate(), in the same
 * group) runs after the dialog closes; `started(name, watcher)` hands on its TaskWatcher.
 */
AppDialog {
    id: root

    property var library: null
    property string targetId
    property string targetGroup
    property string sourceName
    property string iconKey
    signal started(string name, var watcher)
    signal failed()

    readonly property int iconTileSize: Theme.control.heightLg + Theme.space.lg

    function submit() {
        var name = nameField.text.trim()
        if (name.length === 0 || !root.library)
            return
        var watcher = root.library.duplicate(root.targetId, name, root.targetGroup,
                                             savesBox.checked, playtimeBox.checked, root.iconKey)
        root.close()
        if (watcher)
            root.started(name, watcher)
        else
            root.failed()
    }

    size: "sm"
    closeOnPrimary: false
    title: qsTr("Duplicate “%1”").arg(root.sourceName)
    primaryText: qsTr("Duplicate")
    primaryEnabled: !!root.library && nameField.text.trim().length > 0
    onPrimaryClicked: root.submit()

    onOpened: {
        nameField.text = qsTr("%1 (copy)").arg(root.sourceName)
        savesBox.checked = true
        playtimeBox.checked = true
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            AbstractButton {
                id: iconButton
                Layout.preferredWidth: root.iconTileSize
                Layout.preferredHeight: root.iconTileSize
                Layout.alignment: Qt.AlignBottom
                hoverEnabled: true
                enabled: !!root.library && !!root.library.iconsModel
                Accessible.name: qsTr("Change icon")
                ToolTip.visible: hovered
                ToolTip.delay: 400
                ToolTip.text: qsTr("Change icon")
                onClicked: {
                    iconPicker.current = root.iconKey
                    iconPicker.open()
                }
                background: InstanceTile {
                    radius: Theme.radius.sm
                    borderColor: iconButton.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    Behavior on borderColor { ColorAnimation { duration: Theme.motion.fast } }
                }
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: Theme.control.height
                        height: Theme.control.height
                        sourceSize.width: width * 2
                        sourceSize.height: height * 2
                        source: Icons.instanceIcon(root.iconKey)
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.xs
                FieldLabel { text: qsTr("Name") }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    selectByMouse: true
                    onAccepted: root.submit()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            CheckBox {
                id: savesBox
                leftPadding: 0
                text: qsTr("Copy worlds")
            }
            CheckBox {
                id: playtimeBox
                leftPadding: 0
                text: qsTr("Keep play time")
            }
        }
    }

    IconPickerDialog {
        id: iconPicker
        iconsModel: root.library ? root.library.iconsModel : null
        onPicked: (key) => root.iconKey = key
        onOpenFolderRequested: if (root.library) root.library.openFolder(root.library.iconsDir)
    }
}
