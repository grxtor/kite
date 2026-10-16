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
 * so a single delegate does for all of them. Each row shows the logo the
 * file declares for itself (ModFolderModel's iconUrl role: a mod's
 * fabric.mod.json/mods.toml logo, a pack's pack.png); `kind` only picks the
 * glyph shown when it has none, and the noun used when confirming a delete.
 *
 * Laid out like every launcher's mod list: what it is on the left, its
 * on/off switch and a ⋯ menu (also on right-click) on the right.
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
    readonly property int thumbSize: 36
    // The same frame InstanceListRow gives an instance icon.
    readonly property int thumbInset: Theme.space.xs

    function confirmRemove(row, name) {
        confirm.row = row
        confirm.text = qsTr("Remove “%1” from this instance? The file is deleted.").arg(name)
        confirm.open()
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        // One rhythm for every list tab: Theme.space.sm between rows, rows
        // inset by sm on the right and spaced md inside.
        spacing: Theme.space.sm
        boundsBehavior: Flickable.StopAtBounds
        model: root.model
        // Overlays the rows' right edge rather than reserving a gutter, so
        // the rows end where the toolbar above them does.
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
            readonly property string iconUrl: model.iconUrl || ""

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

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space.md
                anchors.rightMargin: Theme.space.sm
                spacing: Theme.space.md

                // The file's own logo when it has one; otherwise (and while
                // it loads) the kind's glyph, so a row never shows an empty
                // gap where a thumbnail would go.
                Rectangle {
                    Layout.preferredWidth: root.thumbSize
                    Layout.preferredHeight: root.thumbSize
                    radius: Theme.radius.sm
                    color: Theme.palette.surfaceSunken
                    opacity: row.itemEnabled ? 1 : Theme.opacity.disabled
                    Image {
                        id: logo
                        anchors.fill: parent
                        anchors.margins: root.thumbInset
                        source: row.iconUrl
                        // The provider scales to exactly this, keeping
                        // pixel-art logos crisp.
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: logo.status !== Image.Ready
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

                Switch {
                    checked: row.itemEnabled
                    enabled: root.unlocked
                    Accessible.name: qsTr("Enable %1").arg(row.name)
                    onToggled: {
                        root.details.setEnabled(root.kind, row.index, checked)
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
                    onTriggered: root.details.setEnabled(root.kind, row.index, !row.itemEnabled)
                }
                MenuSeparator {}
                MenuItem {
                    enabled: root.unlocked
                    text: qsTr("Remove…")
                    icon.source: Icons.url("trash")
                    onTriggered: root.confirmRemove(row.index, row.name)
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
        title: qsTr("Remove")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.details) root.details.remove(root.kind, row)
    }
}
