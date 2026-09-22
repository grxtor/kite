// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's mods: switch each on or off, or remove it. Locked while
 * the game runs -- it has the files open, and a change would only apply to
 * the next launch anyway.
 */
Item {
    id: root

    // InstanceDetails: mods (ModFolderModel: name, version, dateChanged,
    // enabled), modsDir, contentChangesAllowed, setModEnabled, deleteMod.
    property var details: null
    readonly property var model: details ? details.mods : null
    readonly property bool unlocked: !!details && details.contentChangesAllowed
    readonly property int count: list.count
    signal openFolderRequested(string path)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            Text {
                Layout.fillWidth: true
                text: !root.unlocked ? qsTr("The game is running; mods can be changed once it has closed.")
                                     : list.count > 0 ? qsTr("%1 mods").arg(list.count) : ""
                color: root.unlocked ? Theme.palette.textTertiary : Theme.palette.warning
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.details ? root.details.modsDir : "")
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                // Through `model`: one of the roles is called "enabled",
                // which as a delegate property would disable the row itself.
                required property var model
                readonly property string name: model.name
                readonly property var version: model.version
                readonly property bool modEnabled: model.enabled

                width: list.width - Theme.space.md
                height: Theme.control.heightLg + Theme.space.md
                radius: Theme.radius.md
                color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.sm
                    spacing: Theme.space.md

                    Switch {
                        checked: row.modEnabled
                        enabled: root.unlocked
                        Accessible.name: qsTr("Enable %1").arg(row.name)
                        onToggled: {
                            root.details.setModEnabled(row.index, checked)
                            checked = Qt.binding(() => row.modEnabled)
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            width: parent.width
                            text: row.name
                            elide: Text.ElideRight
                            color: row.modEnabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
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
                            confirm.text = qsTr("Remove “%1” from this instance? The file is deleted.").arg(row.name)
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
        title: qsTr("Remove mod")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.details) root.details.deleteMod(row)
    }

    EmptyState {
        anchors.centerIn: parent
        visible: list.count === 0
        title: qsTr("No mods")
        body: root.details && root.details.isMinecraft
              ? qsTr("Drop .jar files into the mods folder, or install a modpack from Discover.")
              : qsTr("This instance cannot have mods.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }
}
