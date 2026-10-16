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
 * than a blocking dialog. Each row's actions sit behind its ⋯ menu (also
 * on right-click); only the on/off switch stays out in the row.
 */
Item {
    id: root

    // InstanceDetails.
    property var details: null
    readonly property var components: root.details ? root.details.components : null
    readonly property var installer: root.details ? root.details.loaderInstaller : null
    readonly property bool locked: !root.details || !root.details.contentChangesAllowed
    readonly property int rowHeight: 56

    // PackProfile reports its failures in launcher terms; the one a player
    // most often meets (the version metadata could not be fetched or
    // checked) is said here in plain words instead. Anything else is shown
    // as it comes -- those already name what failed.
    readonly property string unresolvedError: qsTranslate("ComponentUpdateTask",
        "Instance has unresolved dependencies while loading/checking for launch.")
    function friendlyError(error) {
        return error === root.unresolvedError
               ? qsTr("Some of this instance's version files could not be checked. Check your connection, then reload.")
               : error
    }

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
                // Bordered, like every other toolbar's last button, so its
                // edge is the content edge.
                flat: false
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
                    text: root.components ? root.friendlyError(root.components.lastError) : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
                Button {
                    flat: true
                    visible: !!root.components && root.components.lastError === root.unresolvedError
                    enabled: !root.locked && !root.components.busy
                    text: qsTr("Reload")
                    onClicked: {
                        errorBanner.dismissed = true
                        root.components.reloadProfile()
                    }
                }
                IconButton { iconName: "x"; tip: qsTr("Dismiss"); onClicked: errorBanner.dismissed = true }
            }
        }

        // One card per component, the same rows as Content's -- no panel
        // behind them running empty to the bottom of the page.
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            // The same row rhythm as every list tab (see PackList).
            spacing: Theme.space.sm
            boundsBehavior: Flickable.StopAtBounds
            model: root.components
            // Overlays the rows' right edge rather than reserving a gutter.
            ScrollBar.vertical: ScrollBar {}

            delegate: componentDelegate
        }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: list.count === 0
        title: qsTr("No components")
        body: qsTr("This instance has nothing installed yet.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    // Rows fade out at the fold instead of being sliced by the play bar.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: list
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

            // Whether the ⋯ menu would hold anything at all.
            readonly property bool hasActions: isMoveable || (changeable && hasVersionList)
                                               || (isCustomizable && !isCustom) || isRevertible || isRemovable

            width: list.width
            height: root.rowHeight
            radius: Theme.radius.lg
            color: hover.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
            border.width: 1
            border.color: Theme.palette.border
            Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

            HoverHandler { id: hover }
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: if (row.hasActions) rowMenu.popup()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space.md
                anchors.rightMargin: Theme.space.sm
                spacing: Theme.space.md

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

                IconButton {
                    id: moreButton
                    // Keeps the switches in one column whether or not a row
                    // has a menu.
                    opacity: row.hasActions ? 1 : 0
                    enabled: row.hasActions
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: rowMenu.popup(moreButton, moreButton.width - rowMenu.width, moreButton.height)
                }
            }

            // Hidden entries are zeroed as well: a hidden item still takes
            // its row in a Menu.
            Menu {
                id: rowMenu
                MenuItem {
                    visible: row.changeable && row.hasVersionList
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked
                    text: qsTr("Change version…")
                    icon.source: Icons.url("download")
                    onTriggered: root.openChangeVersion(row.uid)
                }
                MenuItem {
                    visible: row.isCustomizable && !row.isCustom
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked
                    text: qsTr("Customize")
                    icon.source: Icons.url("edit")
                    onTriggered: root.components.customizeComponent(row.index)
                }
                MenuItem {
                    visible: row.isRevertible
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked
                    text: qsTr("Revert to the original…")
                    icon.source: Icons.url("refresh")
                    onTriggered: {
                        revertConfirm.row = row.index
                        revertConfirm.text = qsTr("Revert “%1” to its original version? Your own changes to it are lost.").arg(row.name)
                        revertConfirm.open()
                    }
                }
                MenuItem {
                    visible: row.isMoveable
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked && row.index > 0
                    text: qsTr("Move up")
                    icon.source: Icons.url("arrow-up")
                    onTriggered: root.components.moveComponentUp(row.index)
                }
                MenuItem {
                    visible: row.isMoveable
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked && row.index < list.count - 1
                    text: qsTr("Move down")
                    icon.source: Icons.url("arrow-down")
                    onTriggered: root.components.moveComponentDown(row.index)
                }
                MenuSeparator {
                    visible: row.isRemovable
                    height: visible ? implicitHeight : 0
                }
                MenuItem {
                    visible: row.isRemovable
                    height: visible ? implicitHeight : 0
                    enabled: !root.locked
                    text: qsTr("Remove…")
                    icon.source: Icons.url("trash")
                    onTriggered: {
                        removeConfirm.row = row.index
                        removeConfirm.text = qsTr("Remove “%1” from this instance?").arg(row.name)
                        removeConfirm.open()
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
