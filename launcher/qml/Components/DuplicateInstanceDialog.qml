// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * "Duplicate…": a copy of one instance under a new name, with the copy
 * dialog's choices -- its worlds, its play time, its icon. The QML
 * replacement for CopyInstanceDialog; the copy itself is
 * LibraryController.duplicate() (core), so it lands in the same group as
 * the original.
 *
 * Set targetId, targetGroup, sourceName and iconKey, then open(). The copy
 * runs after the dialog closes; `started(name, watcher)` hands its
 * TaskWatcher to whoever reports how it went.
 */
Dialog {
    id: root

    // LibraryController.
    property var library: null
    property string targetId
    property string targetGroup
    property string sourceName
    property string iconKey
    signal started(string name, var watcher)
    signal failed()

    readonly property int dialogWidth: 460
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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.dialogWidth, parent ? parent.width - Theme.space.xxl * 2 : root.dialogWidth)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Duplicate “%1”").arg(root.sourceName)

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

            // The copy's icon; a click picks another.
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
                background: Rectangle {
                    radius: Theme.radius.sm
                    color: Theme.palette.surfaceSunken
                    border.width: 1
                    border.color: iconButton.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
                }
                contentItem: Item {
                    Image {
                        anchors.centerIn: parent
                        width: Theme.control.height
                        height: Theme.control.height
                        sourceSize.width: width * 2
                        sourceSize.height: height * 2
                        source: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")
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
                // Box edge on the content edge, under the icon.
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

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            highlighted: true
            enabled: !!root.library && nameField.text.trim().length > 0
            text: qsTr("Duplicate")
            onClicked: root.submit()
        }
        Button {
            flat: true
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }

    IconPickerDialog {
        id: iconPicker
        iconsModel: root.library ? root.library.iconsModel : null
        onPicked: (key) => root.iconKey = key
        onOpenFolderRequested: if (root.library) root.library.openFolder(root.library.iconsDir)
    }
}
