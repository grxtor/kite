// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One row per installed file, reused by ContentTab for whichever kind is
 * selected (mods, resource packs, shader packs, texture packs) -- they are
 * all ModFolderModel underneath with the same name/version/enabled roles,
 * so a single delegate does for all of them. `kind` only changes the icon
 * shown (there is no thumbnail data to fall back to) and the noun used
 * when confirming a delete.
 */
Item {
    id: root

    // InstanceDetails, and which of its folder-backed lists this shows.
    property var details: null
    property string kind: "mods"
    property var model: null
    property bool unlocked: false
    property string iconName: "package"

    readonly property int count: list.count

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        spacing: Theme.space.xs
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        ScrollBar.vertical: ScrollBar {}

        delegate: Rectangle {
            id: row
            required property int index
            // Through `model`: one of the roles is called "enabled", which
            // as a delegate property would disable the row itself.
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
                        root.details.setEnabled(root.kind, row.index, checked)
                        checked = Qt.binding(() => row.itemEnabled)
                    }
                }

                // Every kind here is a plain file/folder with no artwork of
                // its own, so the row gets a consistent glyph tile instead
                // of leaving an empty gap where a thumbnail would go.
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radius.md
                    color: Theme.palette.surfaceSunken
                    opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                    MeshIcon {
                        anchors.centerIn: parent
                        iconName: root.iconName
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
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
                        confirm.text = qsTr("Remove “%1” from this instance? The file is deleted.").arg(row.name)
                        confirm.open()
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirm
        property int row: -1
        title: qsTr("Remove")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.details) root.details.remove(root.kind, row)
    }
}
