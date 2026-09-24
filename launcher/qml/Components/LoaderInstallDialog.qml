// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Pick a mod loader and a version of it, and install it - the QML
 * replacement for InstallLoaderDialog. Turning off a conflicting loader
 * (LoaderInstaller.conflictName) is confirmed here rather than asked about
 * silently: the picker itself has no other destructive step.
 */
Dialog {
    id: root

    // LoaderInstaller (InstanceDetails.loaderInstaller).
    property var installer: null
    // Loader uid to open on; empty opens the first one in the list.
    property string preselectUid: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(680, parent ? parent.width - Theme.space.xxl * 2 : 680)
    height: Math.min(600, parent ? parent.height - Theme.space.xxl * 2 : 600)
    modal: true
    title: qsTr("Install a mod loader")

    header: DialogHeader {
        title: root.title
        icon: "download"
    }

    onOpened: {
        list.currentIndex = -1
        if (root.installer) {
            var uid = root.preselectUid.length > 0 ? root.preselectUid
                     : (root.installer.loaders.length > 0 ? root.installer.loaders[0].uid : "")
            if (uid.length > 0) {
                root.installer.selectLoader(uid)
            }
        }
    }

    Connections {
        target: root.installer
        function onSelectedUidChanged() { list.currentIndex = -1 }
    }
    Connections {
        target: root.installer ? root.installer.versions : null
        function onCountChanged() {
            if (list.currentIndex < 0 && root.installer.versions.count > 0) {
                list.currentIndex = 0
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        SegmentedControl {
            Layout.fillWidth: true
            options: root.installer
                     ? root.installer.loaders.map(function (l) { return { value: l.uid, label: l.brandName } })
                     : []
            current: root.installer ? root.installer.selectedUid : ""
            onActivated: (value) => root.installer.selectLoader(value)
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
                visible: !!root.installer && root.installer.supported
                model: root.installer ? root.installer.versions : null
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: cell
                    required property int index
                    required property string versionId
                    required property string version
                    required property bool recommended

                    width: list.width
                    height: Theme.control.heightLg
                    highlighted: ListView.isCurrentItem

                    onClicked: list.currentIndex = index
                    onDoubleClicked: root.doInstall(cell.versionId)

                    contentItem: RowLayout {
                        spacing: Theme.space.sm
                        Text {
                            Layout.fillWidth: true
                            text: cell.version
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                        }
                        StatusBadge {
                            visible: cell.recommended
                            text: qsTr("Recommended")
                            tone: "success"
                        }
                    }
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                visible: running
                running: !!root.installer && !!root.installer.versions && root.installer.versions.loading
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !!root.installer && root.installer.supported && !!root.installer.versions
                         && !root.installer.versions.loading && root.installer.versions.count === 0
                title: qsTr("No versions found")
                body: root.installer && root.installer.versions.error.length > 0
                      ? root.installer.versions.error
                      : qsTr("Nothing is published for this Minecraft version yet.")
                MeshIcon { iconName: "download"; size: 40; color: Theme.palette.textTertiary }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: !!root.installer && !root.installer.supported
                title: qsTr("Not compatible")
                body: root.installer ? root.installer.unsupportedReason : ""
                MeshIcon { iconName: "alert-triangle"; size: 40; color: Theme.palette.textTertiary }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !!root.installer && root.installer.installedVersion.length > 0
            text: root.installer ? qsTr("Already installed: %1").arg(root.installer.installedVersion) : ""
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            text: qsTr("Install")
            enabled: list.currentIndex >= 0
            onClicked: root.doInstall(list.currentItem ? list.currentItem.versionId : "")
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
    }

    function doInstall(versionId) {
        if (!root.installer || versionId.length === 0) {
            return
        }
        if (root.installer.conflictName.length > 0) {
            var picked = root.installer.loaders.find(function (l) { return l.uid === root.installer.selectedUid })
            var brand = picked ? picked.brandName : qsTr("This loader")
            conflictConfirm.versionId = versionId
            conflictConfirm.text = qsTr("%1 and %2 hook into the same parts of the game and cannot run together. Installing this turns %1 off.")
                                        .arg(root.installer.conflictName).arg(brand)
            conflictConfirm.open()
            return
        }
        if (root.installer.install(versionId)) {
            root.close()
        }
    }

    ConfirmDialog {
        id: conflictConfirm
        property string versionId: ""
        title: qsTr("Turn off the other loader?")
        confirmText: qsTr("Install anyway")
        onConfirmed: {
            if (root.installer.install(versionId)) {
                root.close()
            }
        }
    }
}
