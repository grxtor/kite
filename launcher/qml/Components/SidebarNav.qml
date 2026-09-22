// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The left rail: app mark, nav items, account chip pinned to the bottom.
 * Collapses to icons-only below collapseWidth rather than on an explicit
 * toggle, per the brief -- so a caller drives this purely by resizing the
 * rail (e.g. a SplitView handle), with no separate collapse API to keep in
 * sync.
 */
Rectangle {
    id: root

    // Each entry: { id, icon, label }. icon is a short glyph string, same
    // convention as NavItem (see its own header comment).
    property var items: []
    property string currentId: ""

    property string accountName: ""
    property string accountStatus: ""
    property string accountAvatarSource: ""

    signal itemActivated(string id)
    signal accountClicked()

    readonly property int collapseWidth: 180
    readonly property bool collapsed: width < collapseWidth

    implicitWidth: 220
    color: Theme.palette.surface

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.sm
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Rectangle {
                Layout.preferredWidth: Theme.control.heightSm
                Layout.preferredHeight: Theme.control.heightSm
                radius: Theme.radius.md
                color: Theme.palette.accent

                Text {
                    anchors.centerIn: parent
                    text: "M"
                    color: Theme.palette.textOnAccent
                    font.family: Theme.font.family
                    font.weight: Theme.type.bodyStrong.weight
                }
            }

            Text {
                text: "MeshMC"
                visible: !root.collapsed
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
            }
        }

        Repeater {
            model: root.items

            delegate: NavItem {
                Layout.fillWidth: true
                icon: modelData.icon
                label: modelData.label
                selected: modelData.id === root.currentId
                iconOnly: root.collapsed
                onClicked: root.itemActivated(modelData.id)
            }
        }

        // Pushes the account chip to the bottom of the rail regardless of
        // how many nav items are above it.
        Item { Layout.fillHeight: true }

        AccountChip {
            Layout.fillWidth: true
            name: root.accountName
            status: root.accountStatus
            avatarSource: root.accountAvatarSource
            collapsed: root.collapsed
            onClicked: root.accountClicked()
        }
    }
}
