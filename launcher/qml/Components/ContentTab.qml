// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * The instance's installed content: mods, resource packs, shader packs and
 * -- where the instance has them -- core mods (legacy Forge), texture packs
 * (versions before resource packs) and the instance-wide data pack folder
 * (when it is turned on in the instance's settings). One PackList reused
 * across all of them.
 *
 * Filter chips pick the kind and a search narrows the list; ticking rows
 * (see PackList) swaps in a bar of actions for the selection -- enable,
 * disable, remove, export -- in place of nothing, the way a mail or file
 * list does. "Add content" opens the browser for this kind in the tab's
 * place (InstancePage); "Add from file…" sits in More. "Export list…" writes the
 * list as HTML, Markdown, CSV and more. "Check for updates", "Change
 * version…", "Verify dependencies" and "Reset metadata" run through
 * ContentUpdateFlows, with a progress notice while anything downloads.
 *
 * Mods are locked while the game runs, as the old Mods tab was: the files
 * are open, and a change would only apply to the next launch. Packs the
 * game reloads on its own stay editable. Adding a mod to an instance with
 * no loader first offers to install one.
 */
Item {
    id: root

    // InstanceDetails: content (InstanceContentController), loaderInstaller,
    // isMinecraft, name.
    property var details: null
    property string kind: "mods"
    signal openFolderRequested(string path)
    // "Add content": browse for @p kind (InstancePage opens the browser).
    signal browseRequested(string kind)

    readonly property var content: details ? details.content : null
    readonly property bool isMinecraft: !!details && details.isMinecraft

    // Label, noun and empty-state copy per kind.
    readonly property var kindInfo: ({
        mods: { label: qsTr("Mods"), noun: qsTr("mods"), icon: "package",
                emptyTitle: qsTr("No mods"),
                emptyBody: qsTr("Find some with Add content, or drop .jar files here.") },
        coremods: { label: qsTr("Core mods"), noun: qsTr("core mods"), icon: "package",
                    emptyTitle: qsTr("No core mods"),
                    emptyBody: qsTr("Old Forge versions load these before other mods. Most mods go in Mods instead.") },
        resourcepacks: { label: qsTr("Resource packs"), noun: qsTr("resource packs"), icon: "image",
                         emptyTitle: qsTr("No resource packs"),
                         emptyBody: qsTr("Change how the game looks: find one with Add content, or drop a .zip here.") },
        shaderpacks: { label: qsTr("Shader packs"), noun: qsTr("shader packs"), icon: "layers",
                       emptyTitle: qsTr("No shader packs"),
                       emptyBody: qsTr("Needs a shader-capable renderer, such as Iris or OptiFine, to have any effect.") },
        texturepacks: { label: qsTr("Texture packs"), noun: qsTr("texture packs"), icon: "grid",
                        emptyTitle: qsTr("No texture packs"),
                        emptyBody: qsTr("This version predates resource packs and uses texture packs instead.") },
        datapacks: { label: qsTr("Data packs"), noun: qsTr("data packs"), icon: "archive",
                     emptyTitle: qsTr("No data packs"),
                     emptyBody: qsTr("Packs here are added to every world by a loader such as Paxi or OpenLoader.") }
    })

    // One entry per kind this instance has right now.
    readonly property var kinds: root.content
                                 ? root.content.kinds.map((k) => Object.assign({ value: k }, root.kindInfo[k]))
                                 : []
    readonly property var current: root.kindInfo[root.kind] || root.kindInfo.mods

    // The searchable list for the current kind.
    readonly property var listModel: {
        if (!root.content)
            return null
        root.content.kinds // re-asked when a kind appears or goes
        return root.content.model(root.kind)
    }
    readonly property bool unlocked: {
        if (!root.content)
            return false
        root.content.running
        return root.content.changesAllowed(root.kind)
    }
    readonly property string lockedReason: root.unlocked ? ""
        : qsTr("The game is running. %1 can be changed once it closes.").arg(root.current.label)
    // Whether files of this kind can come from Modrinth or CurseForge, and
    // so be updated or changed to another version.
    readonly property bool updatable: !!root.content && !!root.content.updater
                                      && root.content.updater.supportsUpdates(root.kind)
    // Every file of the current kind, searched or not (the tab's badge).
    readonly property int count: root.listModel ? root.listModel.totalCount : 0

    // Shows @p kind when this instance has it (back from Add content, on
    // whatever kind was browsed there).
    function showKind(kind) {
        if (root.kinds.some((k) => k.value === kind))
            root.kind = kind
    }

    // A kind that went away (data packs switched off) falls back to mods.
    onKindsChanged: {
        if (root.kinds.length > 0 && !root.kinds.some((k) => k.value === root.kind))
            root.kind = "mods"
    }

    // What to do once a loader is in place: run when the loader picker
    // closes with one installed, or when it turns up shortly after.
    property var afterLoader: null
    function needsLoaderFirst(action) {
        if (root.kind !== "mods" || !root.content || root.content.hasModLoader)
            return false
        root.afterLoader = action
        loaderPrompt.open()
        return true
    }
    // Runs @p action now, or once a loader is in place for mods.
    function withLoader(action) {
        if (!root.needsLoaderFirst(action))
            action()
    }
    function addFromFile() {
        if (!root.needsLoaderFirst(() => fileDialog.open()))
            fileDialog.open()
    }
    function browse() {
        if (!root.needsLoaderFirst(() => root.browseRequested(root.kind)))
            root.browseRequested(root.kind)
    }

    function fileName(url) {
        const path = decodeURIComponent(String(url))
        return path.slice(path.lastIndexOf("/") + 1)
    }
    function installFiles(urls) {
        if (!root.content || urls.length === 0)
            return
        let added = []
        let failed = []
        for (let i = 0; i < urls.length; ++i) {
            if (root.content.install(root.kind, String(urls[i])))
                added.push(root.fileName(urls[i]))
            else
                failed.push(root.fileName(urls[i]))
        }
        if (failed.length > 0)
            toast.show(failed.length === 1 ? qsTr("“%1” could not be added.").arg(failed[0])
                                           : qsTr("%1 files could not be added.").arg(failed.length), "danger")
        else if (added.length === 1)
            toast.show(qsTr("Added “%1”.").arg(added[0]), "success")
        else
            toast.show(qsTr("Added %1 %2.").arg(added.length).arg(root.current.noun), "success")
    }

    function bulkEnable(enabled) {
        if (root.content)
            root.content.setEnabled(root.kind, packList.selectedKeys, enabled)
    }
    function bulkRemove() {
        const keys = packList.selectedKeys
        bulkConfirm.keys = keys
        bulkConfirm.text = keys.length === 1
            ? qsTr("Remove this file from the instance? It is deleted.")
            : qsTr("Remove %1 %2 from this instance? The files are deleted.").arg(keys.length).arg(root.current.noun)
        bulkConfirm.open()
    }
    function exportList(keys) {
        exportDialog.keys = keys
        exportDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: root.isMinecraft

        // The kind chips on the left of the search while everything fits on
        // one row; below the search row otherwise (Modrinth App's order).
        GridLayout {
            id: toolbar
            Layout.fillWidth: true
            columnSpacing: Theme.space.md
            rowSpacing: Theme.space.md
            columns: oneRow ? 2 : 1

            readonly property int searchMinWidth: 200
            readonly property bool showKinds: root.kinds.length > 1
            readonly property int kindsWidth: showKinds ? kindChips.implicitWidth + columnSpacing : 0
            // The gap between the toolbar's controls (TabToolbar's).
            readonly property int gap: Theme.space.sm
            readonly property int updatesIconWidth: root.updatable ? Theme.control.height + gap : 0
            // The actions row without the search field and the label of
            // "Check for updates".
            readonly property int actionsWidth: folderButton.implicitWidth + moreButton.implicitWidth
                                                + addButton.implicitWidth + gap * 3
            // Measured against the tab's own width, which the toolbar's
            // contents never feed back into.
            readonly property bool oneRow: root.width >= kindsWidth + searchMinWidth + updatesIconWidth + actionsWidth
            readonly property real actionsRowWidth: oneRow ? root.width - kindsWidth : root.width
            // "Check for updates" spelled out when there is room for it as
            // well; just its icon (and tooltip) otherwise.
            readonly property bool updatesLabelFits: actionsRowWidth >= searchMinWidth + updatesMeasure.implicitWidth
                                                                        + gap + actionsWidth

            ChipGroup {
                id: kindChips
                visible: toolbar.showKinds
                Layout.row: toolbar.oneRow ? 0 : 1
                Layout.column: 0
                Layout.alignment: Qt.AlignVCenter
                options: root.kinds.map((k) => ({ value: k.value, label: k.label }))
                current: root.kind
                onActivated: (value) => root.kind = value
            }

            // The toolbar every instance tab shares: search on the left;
            // on the right the plain actions, More, and "Add content" last.
            TabToolbar {
                Layout.row: 0
                Layout.column: toolbar.oneRow && toolbar.showKinds ? 1 : 0
                Layout.fillWidth: true

                leading: SearchBox {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: toolbar.searchMinWidth
                    placeholderText: qsTr("Search %1").arg(root.current.noun)
                }

                // Only measured, for updatesLabelFits.
                Button {
                    id: updatesMeasure
                    visible: false
                    text: qsTr("Check for updates")
                    icon.source: Icons.url("refresh")
                }
                HoverReason {
                    visible: root.updatable
                    reason: root.lockedReason
                    Button {
                        enabled: root.unlocked && root.count > 0 && !root.flowsBusy
                        display: toolbar.updatesLabelFits ? AbstractButton.TextBesideIcon : AbstractButton.IconOnly
                        text: qsTr("Check for updates")
                        icon.source: Icons.url("refresh")
                        ToolTip.visible: hovered && display === AbstractButton.IconOnly
                        ToolTip.delay: 400
                        ToolTip.text: text
                        onClicked: root.withLoader(() => flows.checkUpdates([]))
                    }
                }

                Button {
                    id: folderButton
                    text: qsTr("Open folder")
                    icon.source: Icons.url("folder")
                    onClicked: root.openFolderRequested(root.content ? root.content.dir(root.kind) : "")
                }

                // "More ▾": mirrored so the chevron follows the label.
                Button {
                    id: moreButton
                    LayoutMirroring.enabled: true
                    text: qsTr("More")
                    icon.source: Icons.url("chevron-down")
                    onClicked: moreMenu.popup(moreButton, 0, moreButton.height)

                    Menu {
                        id: moreMenu
                        MenuItem {
                            enabled: root.unlocked
                            text: qsTr("Add from file…")
                            icon.source: Icons.url("file")
                            onTriggered: root.addFromFile()
                        }
                        MenuItem {
                            // A hidden item still takes its row in a Menu; zero it.
                            visible: root.kind === "mods" || root.kind === "coremods"
                            height: visible ? implicitHeight : 0
                            text: qsTr("Open configs folder")
                            icon.source: Icons.url("folder")
                            onTriggered: root.openFolderRequested(root.content ? root.content.configsDir : "")
                        }
                        MenuSeparator {}
                        MenuItem {
                            visible: root.updatable
                            height: visible ? implicitHeight : 0
                            enabled: root.unlocked && root.count > 0 && !root.flowsBusy
                            text: qsTr("Check for updates")
                            icon.source: Icons.url("refresh")
                            onTriggered: root.withLoader(() => flows.checkUpdates([]))
                        }
                        MenuItem {
                            visible: root.kind === "mods"
                            height: visible ? implicitHeight : 0
                            enabled: root.unlocked && root.count > 0 && !root.flowsBusy
                            text: qsTr("Verify dependencies")
                            icon.source: Icons.url("layers")
                            onTriggered: root.withLoader(() => flows.verifyDependencies())
                        }
                        MenuSeparator {
                            visible: root.updatable || root.kind === "mods"
                            height: visible ? implicitHeight : 0
                        }
                        MenuItem {
                            enabled: root.listModel && root.listModel.count > 0
                            text: qsTr("Select all")
                            icon.source: Icons.url("check")
                            onTriggered: packList.selectAll()
                        }
                        MenuItem {
                            enabled: root.count > 0
                            text: qsTr("Export list…")
                            icon.source: Icons.url("list")
                            onTriggered: root.exportList([])
                        }
                    }
                }

                // The tab's main action, last on the right like Modrinth
                // App's "Add content" -- still a secondary button: the play
                // bar's Play is this page's one primary action. Browsing
                // works while the game runs; installing then waits.
                Button {
                    id: addButton
                    text: qsTr("Add content")
                    icon.source: Icons.url("plus")
                    onClicked: root.browse()
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !root.unlocked
            text: root.lockedReason
        }

        // An update, version change or missing-dependency install on its
        // way.
        NoticeBar {
            Layout.fillWidth: true
            visible: root.flowsBusy
            tone: "info"
            text: flows.busyText
            progress: flows.busyProgress
        }

        // What can be done with the ticked rows.
        ContentSelectionBar {
            Layout.fillWidth: true
            visible: packList.selecting
            list: packList
            listModel: root.listModel
            unlocked: root.unlocked
            lockedReason: root.lockedReason
            onEnableRequested: (enabled) => root.bulkEnable(enabled)
            onRemoveRequested: root.bulkRemove()
            onExportRequested: root.exportList(packList.selectedKeys)
            updatable: root.updatable
            onCheckUpdatesRequested: root.withLoader(() => flows.checkUpdates(packList.selectedKeys))
            onResetMetadataRequested: flows.resetMetadata(packList.selectedKeys, "")
        }

        PackList {
            id: packList
            Layout.fillWidth: true
            Layout.fillHeight: true
            controller: root.content
            kind: root.kind
            model: root.listModel
            unlocked: root.unlocked
            lockedReason: root.lockedReason
            iconName: root.current.icon
            noun: root.current.noun
            onOpenFolderRequested: (path) => root.openFolderRequested(path)
            updatable: root.updatable
            onChangeVersionRequested: (key) => root.withLoader(() => flows.changeVersion(key))
            onResetMetadataRequested: (key, name) => flows.resetMetadata([key], name)
        }
    }

    readonly property bool flowsBusy: flows.busyText.length > 0
    ContentUpdateFlows {
        id: flows
        content: root.content
        kind: root.kind
        noun: root.current.noun
        onNotify: (message, tone) => { if (message.length > 0) toast.show(message, tone) }
    }

    // The search follows whichever list is showing.
    Binding {
        target: root.listModel
        property: "filterText"
        value: searchField.text
        when: !!root.listModel
    }

    // Escape backs out of a selection, as in any list.
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.visible && packList.selecting && !searchField.activeFocus
                 && !exportDialog.visible && !bulkConfirm.visible
        onActivated: packList.clearSelection()
    }

    // Files dropped on the tab go into the current kind.
    DropArea {
        anchors.fill: parent
        enabled: root.isMinecraft && root.unlocked
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (!drop.hasUrls)
                return
            const urls = drop.urls
            if (!root.needsLoaderFirst(() => root.installFiles(urls)))
                root.installFiles(urls)
            drop.accept(Qt.CopyAction)
        }
    }

    // No button of its own: "Add content" is already in the toolbar right
    // above it.
    // EmptyState takes its title's width unless given one of its own.
    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: root.isMinecraft && root.count === 0
        title: root.current.emptyTitle
        body: root.current.emptyBody
        MeshIcon { iconName: root.current.icon; size: 40; color: Theme.palette.textTertiary }
    }

    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: !root.isMinecraft
        title: qsTr("No content")
        body: qsTr("This instance cannot have mods, resource packs or shader packs.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Add %1").arg(root.current.noun)
        fileMode: FileDialog.OpenFiles
        nameFilters: root.kind === "mods" || root.kind === "coremods"
                     ? [qsTr("Mod files (*.jar *.zip *.litemod)"), qsTr("All files (*)")]
                     : [qsTr("Pack files (*.zip)"), qsTr("All files (*)")]
        onAccepted: root.installFiles(selectedFiles)
    }

    ConfirmDialog {
        id: bulkConfirm
        property var keys: []
        title: qsTr("Remove")
        confirmText: qsTr("Remove")
        onConfirmed: {
            const removed = root.content ? root.content.remove(root.kind, keys) : 0
            packList.clearSelection()
            if (removed < keys.length)
                toast.show(qsTr("Some files could not be removed. Is the game running?"), "danger")
        }
    }

    // Mods need a loader; offer the loader picker rather than an
    // instruction (ModFolderPage::ensureModLoaderPresent()).
    ConfirmDialog {
        id: loaderPrompt
        title: qsTr("Install a mod loader?")
        text: qsTr("Mods need a loader such as Fabric or Forge to run, and this instance has none.")
        confirmText: qsTr("Choose a loader…")
        danger: false
        onConfirmed: loaderDialog.open()
    }

    LoaderInstallDialog {
        id: loaderDialog
        installer: root.details ? root.details.loaderInstaller : null
        // Installing resolves the new component in the background, so the
        // loader may only show up a moment after this closes.
        onClosed: {
            if (root.content && root.content.hasModLoader)
                root.runAfterLoader()
            else
                afterLoaderExpiry.restart()
        }
    }
    function runAfterLoader() {
        const action = root.afterLoader
        root.afterLoader = null
        afterLoaderExpiry.stop()
        if (action)
            action()
    }
    // Long enough for the new loader's metadata to resolve.
    readonly property int loaderSettleMs: 15000
    Timer {
        id: afterLoaderExpiry
        interval: root.loaderSettleMs
        onTriggered: root.afterLoader = null
    }
    Connections {
        target: root.content
        function onHasModLoaderChanged() {
            if (root.content.hasModLoader && afterLoaderExpiry.running)
                root.runAfterLoader()
        }
    }

    ExportContentListDialog {
        id: exportDialog
        controller: root.content
        kind: root.kind
        noun: root.current.noun
        listName: root.details ? root.details.name : ""
        onSaved: toast.show(qsTr("List saved."), "success")
        onCopied: toast.show(qsTr("List copied."), "success")
        onFailed: (message) => toast.show(message, "danger")
    }

    Toast {
        id: toast
        parent: Overlay.overlay
    }
}
