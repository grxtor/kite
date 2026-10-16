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
 * A search narrows the list; ticking rows (see PackList) swaps in a bar of
 * actions for the selection -- enable, disable, remove, export -- in place
 * of nothing, the way a mail or file list does. "Export list…" writes the
 * list as HTML, Markdown, CSV and more.
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
    signal browseRequested()

    readonly property var content: details ? details.content : null
    readonly property bool isMinecraft: !!details && details.isMinecraft

    // Label, noun and empty-state copy per kind.
    readonly property var kindInfo: ({
        mods: { label: qsTr("Mods"), noun: qsTr("mods"), icon: "package",
                emptyTitle: qsTr("No mods"),
                emptyBody: qsTr("Drop .jar files here, add one from a file, or install a modpack from Discover.") },
        coremods: { label: qsTr("Core mods"), noun: qsTr("core mods"), icon: "package",
                    emptyTitle: qsTr("No core mods"),
                    emptyBody: qsTr("Old Forge versions load these before other mods. Most mods go in Mods instead.") },
        resourcepacks: { label: qsTr("Resource packs"), noun: qsTr("resource packs"), icon: "image",
                         emptyTitle: qsTr("No resource packs"),
                         emptyBody: qsTr("Change how the game looks: add one from a file, or browse for one.") },
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
    // Every file of the current kind, searched or not (the tab's badge).
    readonly property int count: root.listModel ? root.listModel.totalCount : 0

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
    function addFromFile() {
        if (!root.needsLoaderFirst(() => fileDialog.open()))
            fileDialog.open()
    }
    function browse() {
        if (!root.needsLoaderFirst(() => root.browseRequested()))
            root.browseRequested()
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

        RowLayout {
            id: toolbar
            Layout.fillWidth: true
            spacing: Theme.space.md

            readonly property int searchMinWidth: 200
            readonly property int kindBoxWidth: 180
            // Every kind as a segment while they fit beside a usable search
            // field; a drop-down once they do not (legacy instances have
            // up to five kinds). Measured against the tab's own width,
            // which the toolbar's contents never feed back into.
            readonly property bool kindsFit: root.width >= kindPicker.implicitWidth + searchMinWidth
                                                           + moreButton.implicitWidth + addReason.implicitWidth
                                                           + spacing * 3

            SegmentedControl {
                id: kindPicker
                visible: toolbar.kindsFit
                options: root.kinds.map((k) => ({ value: k.value, label: k.label }))
                current: root.kind
                onActivated: (value) => root.kind = value
            }
            ComboBox {
                id: kindBox
                visible: !toolbar.kindsFit
                Layout.preferredWidth: toolbar.kindBoxWidth
                model: root.kinds
                textRole: "label"
                valueRole: "value"
                currentIndex: root.kinds.findIndex((k) => k.value === root.kind)
                Accessible.name: qsTr("Content type")
                onActivated: {
                    root.kind = currentValue
                    currentIndex = Qt.binding(() => root.kinds.findIndex((k) => k.value === root.kind))
                }
            }

            SearchBox {
                id: searchField
                Layout.fillWidth: true
                Layout.minimumWidth: toolbar.searchMinWidth
                placeholderText: qsTr("Search %1").arg(root.current.noun)
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
                        text: qsTr("Browse online…")
                        icon.source: Icons.url("globe")
                        onTriggered: root.browse()
                    }
                    MenuItem {
                        text: qsTr("Open folder")
                        icon.source: Icons.url("folder")
                        onTriggered: root.openFolderRequested(root.content ? root.content.dir(root.kind) : "")
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

            // Secondary, like the rest of the toolbar: the play bar's Play
            // is this page's one primary action.
            HoverReason {
                id: addReason
                reason: root.lockedReason
                Button {
                    enabled: root.unlocked
                    text: qsTr("Add from file…")
                    icon.source: Icons.url("plus")
                    onClicked: root.addFromFile()
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !root.unlocked
            text: root.lockedReason
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
        }
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

    // No button of its own: "Add from file…" is already in the toolbar
    // right above it.
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
