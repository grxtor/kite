// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Pick a Minecraft version for this instance. Snapshots and old versions are
 * hidden by default.
 */
AppDialog {
    id: root

    property var details: null
    // Not a binding to details.minecraftVersions: the getter starts the version download, and this
    // dialog is built eagerly. Set from onOpened.
    property var versions: null

    height: Math.min(600, parent ? parent.height - Theme.space.xxl * 2 : 600)
    closeOnOutsidePress: true
    closeOnPrimary: false
    title: qsTr("Change Minecraft version")
    primaryText: qsTr("Change")
    primaryEnabled: list.currentIndex >= 0
    onPrimaryClicked: root.doChange(list.currentItem ? list.currentItem.versionId : "")

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

        BoxSurface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sunken: true

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
                        Copy {
                            Layout.fillWidth: true
                            text: cell.version
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
                MeshIcon { iconName: "cube"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
            }
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
