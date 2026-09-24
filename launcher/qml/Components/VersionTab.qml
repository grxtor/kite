// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance's components: Minecraft, a mod loader, LWJGL, and anything
 * else installed alongside them - the widget-free replacement for
 * VersionPage. Long operations (installing/updating a component) run
 * through PackProfile's own `task`, shown as a thin progress bar rather
 * than a blocking dialog.
 */
Item {
    id: root

    // InstanceDetails.
    property var details: null
    readonly property var components: root.details ? root.details.components : null
    readonly property var installer: root.details ? root.details.loaderInstaller : null
    readonly property bool locked: !root.details || !root.details.contentChangesAllowed

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Text {
                Layout.fillWidth: true
                text: list.count > 0 ? qsTr("%1 components").arg(list.count) : ""
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                enabled: !root.locked && !!root.components && !root.components.busy
                text: qsTr("Change Minecraft version")
                onClicked: minecraftVersionDialog.open()
            }
            Button {
                enabled: !root.locked && !!root.installer && !root.components.busy
                text: qsTr("Install loader")
                icon.source: Icons.url("download")
                onClicked: {
                    loaderInstallDialog.preselectUid = ""
                    loaderInstallDialog.open()
                }
            }
            IconButton {
                iconName: "refresh"
                tip: qsTr("Reload")
                enabled: !root.locked && !!root.components && !root.components.busy
                onClicked: root.components.reloadProfile()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: !!root.components && root.components.busy
            implicitHeight: statusRow.implicitHeight + Theme.space.sm * 2
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ColumnLayout {
                id: statusRow
                anchors.fill: parent
                anchors.margins: Theme.space.sm
                spacing: Theme.space.xs

                Text {
                    Layout.fillWidth: true
                    text: root.components && root.components.task
                          ? (root.components.task.status.length > 0 ? root.components.task.status
                             : (root.components.task.title.length > 0 ? root.components.task.title : qsTr("Updating…")))
                          : qsTr("Updating…")
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
                LaunchProgressBar {
                    Layout.fillWidth: true
                    progress: root.components && root.components.task ? root.components.task.progress : -1
                }
            }
        }

        Rectangle {
            id: errorBanner
            Layout.fillWidth: true
            property bool dismissed: false
            visible: !dismissed && !!root.components && root.components.lastError.length > 0
            implicitHeight: Theme.control.heightLg
            radius: Theme.radius.md
            color: Theme.palette.dangerSubtle
            border.width: 1
            border.color: Theme.palette.danger

            Connections {
                target: root.components
                function onLastErrorChanged() { errorBanner.dismissed = false }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space.md
                anchors.rightMargin: Theme.space.xs
                spacing: Theme.space.sm

                MeshIcon { iconName: "alert-triangle"; size: Theme.icon.sm; color: Theme.palette.danger }
                Text {
                    Layout.fillWidth: true
                    text: root.components ? root.components.lastError : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
                IconButton { iconName: "x"; tip: qsTr("Dismiss"); onClicked: errorBanner.dismissed = true }
            }
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
                spacing: Theme.space.xs
                boundsBehavior: Flickable.StopAtBounds
                model: root.components
                ScrollBar.vertical: ScrollBar {}

                delegate: componentDelegate
            }

            EmptyState {
                anchors.centerIn: parent
                visible: list.count === 0
                title: qsTr("No components")
                body: qsTr("This instance has nothing installed yet.")
                MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    Component {
        id: componentDelegate

        Rectangle {
            id: row
            required property int index
            required property string name
            required property string version
            required property string uid
            required property string problemSeverity
            required property bool isCustom
            required property bool isEnabled
            required property bool canDisable
            required property bool isRemovable
            required property bool isMoveable
            required property bool isCustomizable
            required property bool isRevertible
            required property bool hasVersionList

            readonly property bool changeable: uid === "net.minecraft" || root.loaderUids.indexOf(uid) >= 0

            width: list.width
            height: 56
            radius: Theme.radius.md
            color: hover.hovered ? Theme.palette.surfaceRaised : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

            HoverHandler { id: hover }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space.sm
                anchors.rightMargin: Theme.space.sm
                spacing: Theme.space.sm

                MeshIcon {
                    visible: row.problemSeverity === "warning" || row.problemSeverity === "error"
                    iconName: "alert-triangle"
                    size: Theme.icon.sm
                    color: row.problemSeverity === "error" ? Theme.palette.danger : Theme.palette.warning
                }

                Column {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: Theme.space.xxs

                    Text {
                        width: parent.width
                        text: row.name
                        elide: Text.ElideRight
                        color: row.isEnabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.bodyStrong.pixelSize
                        font.weight: Theme.type.bodyStrong.weight
                    }
                    Row {
                        spacing: Theme.space.xs
                        Tag { text: row.version }
                        StatusBadge { visible: row.isCustom; text: qsTr("Custom"); tone: "warning" }
                        StatusBadge { visible: !row.isEnabled; text: qsTr("Off") }
                    }
                }

                RowLayout {
                    visible: hover.hovered
                    spacing: Theme.space.xxs

                    IconButton {
                        visible: row.isMoveable
                        enabled: !root.locked && row.index > 0
                        iconName: "arrow-up"
                        tip: qsTr("Move up")
                        onClicked: root.components.moveComponentUp(row.index)
                    }
                    IconButton {
                        visible: row.isMoveable
                        enabled: !root.locked && row.index < list.count - 1
                        iconName: "arrow-down"
                        tip: qsTr("Move down")
                        onClicked: root.components.moveComponentDown(row.index)
                    }
                    IconButton {
                        visible: row.changeable && row.hasVersionList
                        enabled: !root.locked
                        iconName: "download"
                        tip: qsTr("Change version")
                        onClicked: root.openChangeVersion(row.uid)
                    }
                    IconButton {
                        visible: row.isCustomizable && !row.isCustom
                        enabled: !root.locked
                        iconName: "edit"
                        tip: qsTr("Customize")
                        onClicked: root.components.customizeComponent(row.index)
                    }
                    IconButton {
                        visible: row.isRevertible
                        enabled: !root.locked
                        iconName: "refresh"
                        tip: qsTr("Revert to the original")
                        onClicked: {
                            revertConfirm.row = row.index
                            revertConfirm.text = qsTr("Revert “%1” to its original version? Your own changes to it are lost.").arg(row.name)
                            revertConfirm.open()
                        }
                    }
                    IconButton {
                        visible: row.isRemovable
                        enabled: !root.locked
                        iconName: "trash"
                        tip: qsTr("Remove")
                        onClicked: {
                            removeConfirm.row = row.index
                            removeConfirm.text = qsTr("Remove “%1” from this instance?").arg(row.name)
                            removeConfirm.open()
                        }
                    }
                }

                Switch {
                    visible: row.canDisable
                    enabled: !root.locked
                    checked: row.isEnabled
                    Accessible.name: qsTr("Enable %1").arg(row.name)
                    onToggled: {
                        root.components.setComponentEnabled(row.uid, checked)
                        checked = Qt.binding(() => row.isEnabled)
                    }
                }
            }
        }
    }

    // Every loader uid this launcher knows how to install - used to decide
    // whether a row's uid can open the loader dialog (vs. the Minecraft
    // version dialog, vs. no "change version" action at all for a
    // component this tab does not have a picker for, e.g. LWJGL).
    readonly property var loaderUids: root.installer ? root.installer.loaders.map(function (l) { return l.uid }) : []

    function openChangeVersion(uid) {
        if (uid === "net.minecraft") {
            minecraftVersionDialog.open()
        } else if (root.installer) {
            loaderInstallDialog.preselectUid = uid
            loaderInstallDialog.open()
        }
    }

    MinecraftVersionDialog {
        id: minecraftVersionDialog
        details: root.details
    }

    LoaderInstallDialog {
        id: loaderInstallDialog
        installer: root.installer
    }

    ConfirmDialog {
        id: removeConfirm
        property int row: -1
        title: qsTr("Remove component")
        confirmText: qsTr("Remove")
        onConfirmed: if (root.components) root.components.removeComponent(row)
    }

    ConfirmDialog {
        id: revertConfirm
        property int row: -1
        title: qsTr("Revert component")
        confirmText: qsTr("Revert")
        onConfirmed: if (root.components) root.components.revertComponent(row)
    }
}
