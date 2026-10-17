// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * The instance's components: Minecraft, a mod loader, LWJGL, and anything
 * else installed alongside them - the widget-free replacement for
 * VersionPage. Long operations (installing/updating a component) run
 * through PackProfile's own `task`, shown as a thin progress bar rather
 * than a blocking dialog. Each row's actions sit behind its ⋯ menu (also
 * on right-click); only the on/off switch stays out in the row. The
 * toolbar's "More" menu holds the rarer actions: reloading, an empty component,
 * jar mods, a replacement Minecraft.jar, downloading the game files again,
 * and the libraries and game folders.
 */
Item {
    id: root

    // InstanceDetails.
    property var details: null
    readonly property var components: root.details ? root.details.components : null
    readonly property var installer: root.details ? root.details.loaderInstaller : null
    readonly property bool locked: !root.details || !root.details.contentChangesAllowed
    // ComponentTools: the actions behind "More" and a row's "Edit JSON".
    readonly property var tools: root.details ? root.details.componentTools : null
    readonly property string lockedReason: root.locked ? qsTr("The game is running. Components can be changed once it closes.") : ""
    // The TaskWatcher of "Download all files", while it runs and after it fails.
    property var downloadWatcher: null
    readonly property bool downloading: !!root.downloadWatcher && root.downloadWatcher.running
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

        TabToolbar {
            Layout.fillWidth: true

            Button {
                enabled: !root.locked && !!root.components && !root.components.busy
                text: qsTr("Change Minecraft version")
                onClicked: minecraftVersionDialog.open()
            }
            // "More ▾": mirrored so the chevron follows the label.
            HoverReason {
                visible: !!root.tools
                reason: root.lockedReason
                Button {
                    id: moreButton
                    LayoutMirroring.enabled: true
                    text: qsTr("More")
                    icon.source: Icons.url("chevron-down")
                    onClicked: moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height)
                }
            }
            // Secondary, like the rest of the toolbar: the play bar's Play
            // is this page's one primary action.
            Button {
                enabled: !root.locked && !!root.installer && !!root.components && !root.components.busy
                text: qsTr("Install loader")
                icon.source: Icons.url("download")
                onClicked: {
                    loaderInstallDialog.preselectUid = ""
                    loaderInstallDialog.open()
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: root.locked && !!root.details
            text: root.lockedReason
        }

        // The result of "Download all files" (progress, or why it failed) and
        // anything else the extra actions have to say.
        NoticeBar {
            Layout.fillWidth: true
            readonly property bool failed: !!root.downloadWatcher && root.downloadWatcher.failed
            visible: root.downloading || failed || (!!root.tools && root.tools.error.length > 0)
            tone: root.downloading ? "info" : "warning"
            progress: root.downloading ? root.downloadWatcher.progress : -2
            text: root.downloading ? (root.downloadWatcher.status.length > 0 ? root.downloadWatcher.status : qsTr("Downloading game files…"))
                  : failed ? (root.downloadWatcher.error.length > 0 ? root.downloadWatcher.error : qsTr("Downloading the game files failed."))
                  : (root.tools ? root.tools.error : "")
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
                    enabled: !root.locked && !!root.components && !root.components.busy
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

            // Whether this component has a patch file to edit; refreshed
            // when its menu opens.
            property bool hasFile: false
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
                // Looked up each time the menu opens: customizing a
                // component creates its file.
                onAboutToShow: row.hasFile = !!root.tools && root.tools.hasJsonFile(row.index)
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
                    visible: row.hasFile
                    height: visible ? implicitHeight : 0
                    text: qsTr("Edit JSON")
                    icon.source: Icons.url("edit")
                    onTriggered: root.tools.editComponentJson(row.index)
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

    // For review snapshots (Main.qml's "review=<name>" dev route, through
    // InstancePage): the More menu, or the add-a-component dialog.
    function review(name) {
        if (name === "versionmore")
            moreMenu.popup(moreButton, moreButton.width - moreMenu.width, moreButton.height)
        else if (name === "newcomponent")
            newComponentDialog.open()
    }

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

    Menu {
        id: moreMenu
        MenuItem {
            enabled: !root.locked && !!root.components && !root.components.busy
            text: qsTr("Reload")
            icon.source: Icons.url("refresh")
            onTriggered: root.components.reloadProfile()
        }
        MenuSeparator {}
        MenuItem {
            enabled: !root.locked && !!root.components && !root.components.busy
            text: qsTr("Add empty component…")
            icon.source: Icons.url("plus")
            onTriggered: newComponentDialog.open()
        }
        MenuItem {
            enabled: !root.locked && !!root.components && !root.components.busy
            text: qsTr("Add to Minecraft.jar…")
            icon.source: Icons.url("archive")
            onTriggered: jarModsDialog.open()
        }
        MenuItem {
            enabled: !root.locked && !!root.components && !root.components.busy
            text: qsTr("Replace Minecraft.jar…")
            icon.source: Icons.url("archive")
            onTriggered: mainJarDialog.open()
        }
        MenuSeparator {}
        MenuItem {
            enabled: !root.locked && !root.downloading
            text: qsTr("Download all game files")
            icon.source: Icons.url("download")
            onTriggered: root.downloadWatcher = root.tools.downloadAll()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Open libraries folder")
            icon.source: Icons.url("folder")
            onTriggered: root.tools.openLibrariesFolder()
        }
        MenuItem {
            text: qsTr("Open game folder")
            icon.source: Icons.url("folder")
            onTriggered: root.tools.openGameFolder()
        }
    }

    NewComponentDialog {
        id: newComponentDialog
        tools: root.tools
    }

    FileDialog {
        id: jarModsDialog
        title: qsTr("Choose jar mods")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Minecraft.jar mods (*.zip *.jar)")]
        onAccepted: root.tools.addJarMods(selectedFiles)
    }

    FileDialog {
        id: mainJarDialog
        title: qsTr("Choose the replacement Minecraft.jar")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Minecraft.jar replacement (*.jar)")]
        onAccepted: root.tools.replaceMainJar(selectedFile)
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
