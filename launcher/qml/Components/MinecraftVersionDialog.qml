// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Pick a Minecraft version for this instance - the QML replacement for the
 * plain VersionSelectDialog VersionPage's "Change version" opens for the
 * net.minecraft component. Snapshots and old versions are hidden by
 * default, the same starting point VanillaPage gives a new instance.
 */
Dialog {
    id: root

    // InstanceDetails.
    property var details: null
    // Not a binding to details.minecraftVersions: that getter builds the
    // proxy and starts its download the first time anything reads it (see
    // InstanceDetails::minecraftVersions()), and this dialog is
    // instantiated eagerly with the rest of VersionTab.qml's children -
    // reading it here would fetch the version list every time the
    // instance page opens, whether or not this dialog ever does. Set from
    // onOpened below instead, so opening the tab stays network-free -
    // mirrors ContentBrowserView.qml's own visible-gated first search.
    property var versions: null

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - Theme.space.xxl * 2 : 560)
    height: Math.min(600, parent ? parent.height - Theme.space.xxl * 2 : 600)
    modal: true
    title: qsTr("Change Minecraft version")

    header: DialogHeader {
        title: root.title
        icon: "cube"
    }

    onOpened: {
        list.currentIndex = -1
        if (root.details) {
            root.versions = root.details.minecraftVersions
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            CheckBox {
                text: qsTr("Snapshots")
                checked: !!root.versions && root.versions.showSnapshots
                onToggled: if (root.versions) root.versions.showSnapshots = checked
            }
            CheckBox {
                text: qsTr("Old versions")
                checked: !!root.versions && root.versions.showOldVersions
                onToggled: if (root.versions) root.versions.showOldVersions = checked
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.versions
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: cell
                    required property int index
                    required property string versionId
                    required property string version
                    required property string type

                    width: list.width
                    height: Theme.control.heightLg
                    highlighted: ListView.isCurrentItem

                    onClicked: list.currentIndex = index
                    onDoubleClicked: root.doChange(cell.versionId)

                    contentItem: RowLayout {
                        spacing: Theme.space.sm
                        Text {
                            Layout.fillWidth: true
                            text: cell.version
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                        }
                        Tag { visible: cell.type !== "release"; text: cell.type }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                visible: running
                running: !!root.versions && root.versions.loading
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !!root.versions && !root.versions.loading && root.versions.count === 0
                title: qsTr("No versions found")
                body: root.versions && root.versions.error.length > 0
                      ? root.versions.error
                      : qsTr("Try turning on snapshots or old versions.")
                MeshIcon { iconName: "cube"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            text: qsTr("Change")
            enabled: list.currentIndex >= 0
            onClicked: root.doChange(list.currentItem ? list.currentItem.versionId : "")
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
    }

    function doChange(versionId) {
        if (!root.details || !root.details.components || versionId.length === 0) {
            return
        }
        if (root.details.components.changeComponentVersion("net.minecraft", versionId)) {
            root.close()
        }
    }
}
