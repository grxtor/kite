// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The left rail: brand, the main destinations, the last few instances
 * played, and the account. Pages that open elsewhere (Settings, for now a
 * dialog) are still listed here -- where they open is the shell's business,
 * not the rail's.
 */
Rectangle {
    id: root

    // Each entry: { id, icon, label }; icon is a MeshIcon name.
    property var items: []
    property var footerItems: []
    property string currentId: ""

    // Any model with instanceId/name/iconKey/isRunning roles; only the
    // first `recentLimit` rows are shown.
    property var recentModel: null
    property int recentLimit: 4

    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""

    signal itemActivated(string id)
    signal recentActivated(string id)
    signal recentPlayRequested(string id)
    signal accountClicked()

    implicitWidth: 244
    color: Theme.palette.surface

    // Hairline against the page, instead of a contrasting fill.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.divider
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.md
        anchors.rightMargin: Theme.space.md + 1
        spacing: Theme.space.xs

        // Brand
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.control.heightLg
            Layout.leftMargin: Theme.space.xs
            Layout.bottomMargin: Theme.space.md
            spacing: Theme.space.sm + 2

            Image {
                readonly property int extent: Theme.control.heightSm + 2
                Layout.preferredWidth: extent
                Layout.preferredHeight: extent
                source: "qrc:/icons/multimc/scalable/instances/meshmc.svg"
                // From a constant, not from width: the layout sizes this
                // item from its implicit size, which sourceSize sets.
                sourceSize: Qt.size(extent * 2, extent * 2)
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: "MeshMC"
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize + 2
                font.weight: Font.Bold
                font.letterSpacing: -0.2
            }

            Item { Layout.fillWidth: true }
        }

        Repeater {
            model: root.items
            delegate: NavItem {
                required property var modelData
                Layout.fillWidth: true
                iconName: modelData.icon
                label: modelData.label
                selected: modelData.id === root.currentId
                onClicked: root.itemActivated(modelData.id)
            }
        }

        Text {
            Layout.topMargin: Theme.space.xl
            Layout.leftMargin: Theme.space.md
            Layout.bottomMargin: Theme.space.xs
            visible: recentRepeater.count > 0
            text: qsTr("RECENT")
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.overline.pixelSize
            font.weight: Theme.type.overline.weight
            font.letterSpacing: Theme.type.overline.letterSpacing
        }

        Repeater {
            id: recentRepeater
            model: root.recentModel
            delegate: RecentItem {
                required property int index

                // Rows past the limit stay out of the layout entirely.
                visible: index < root.recentLimit
                Layout.fillWidth: true
                onClicked: root.recentActivated(instanceId)
                onPlayRequested: root.recentPlayRequested(instanceId)
            }
        }

        Item { Layout.fillHeight: true }

        Repeater {
            model: root.footerItems
            delegate: NavItem {
                required property var modelData
                Layout.fillWidth: true
                iconName: modelData.icon
                label: modelData.label
                selected: modelData.id === root.currentId
                onClicked: root.itemActivated(modelData.id)
            }
        }

        AccountChip {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space.sm
            name: root.accountName
            kind: root.accountKind
            avatarSource: root.accountAvatarSource
            onClicked: root.accountClicked()
        }
    }
}
