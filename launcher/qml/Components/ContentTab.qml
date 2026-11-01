// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * The instance's installed content: one searchable PackList across mods, packs, shaders and data packs.
 * Mods are locked while the game runs: their files are open and a change would only apply to the next launch.
 */
Item {
    id: root

    // InstanceDetails.
    property var details: null
    property var shell: null
    property string kind: "mods"
    signal openFolderRequested(string path)
    signal browseRequested(string kind)
    signal openInstanceRequested(string id)

    readonly property var content: details ? details.content : null
    readonly property var health: root.content ? root.content.health : null
    readonly property bool isMinecraft: !!details && details.isMinecraft
    property bool performance: false
    readonly property string instanceId: details ? details.instanceId : ""
    readonly property string gameVersion: details && details.contentBrowser ? details.contentBrowser.instanceGameVersion : ""
    readonly property bool speedUpOffered: !!root.content && (root.content.hasModLoader
        || (!!root.shell && root.shell.performanceSupported(root.gameVersion)))
    readonly property string instanceLoader: details && details.contentBrowser ? details.contentBrowser.instanceLoader : ""

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

    readonly property var kinds: root.content
                                 ? root.content.kinds.map((k) => Object.assign({ value: k }, root.kindInfo[k]))
                                 : []
    readonly property var current: root.kindInfo[root.kind] || root.kindInfo.mods

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
    readonly property bool updatable: !!root.content && !!root.content.updater
                                      && root.content.updater.supportsUpdates(root.kind)
    readonly property int count: root.listModel ? root.listModel.totalCount : 0

    function showKind(kind) {
        if (root.kinds.some((k) => k.value === kind))
            root.kind = kind
    }

    onKindsChanged: {
        if (root.kinds.length > 0 && !root.kinds.some((k) => k.value === root.kind))
            root.kind = "mods"
    }

    property var afterLoader: null
    function needsLoaderFirst(action) {
        if (root.kind !== "mods" || !root.content || root.content.hasModLoader)
            return false
        root.afterLoader = action
        loaderPrompt.open()
        return true
    }
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
    // rows: ModHealthModel.installRows() maps of key, dependencyId and replacesKey.
    function installMissing(rows) {
        healthDialog.close()
        root.withLoader(() => flows.installMissing(rows.map((r) => r.key), rows.map((r) => r.dependencyId),
                                                   rows.map((r) => r.replacesKey)))
    }
    // A ModHealthDialog row action; for installs @p key is the file that needs it.
    function runHealthAction(action, key, dependencyId, replacesKey) {
        if (action === "install")
            root.installMissing([{ key: key, dependencyId: dependencyId, replacesKey: replacesKey }])
        else if (root.content)
            root.content.setEnabled("mods", [key], action === "enable")
    }
    function refreshPerformance() {
        root.performance = !!root.shell && root.instanceId.length > 0
                           && root.shell.performanceEnabled(root.instanceId)
    }
    // Picks up a Kite Performance setup the New instance dialog started.
    function trackRunningSetup() {
        const watcher = root.shell && root.instanceId.length > 0 ? root.shell.performanceSetup(root.instanceId) : null
        if (watcher && flows.activity !== watcher)
            flows.track(watcher, "")
    }
    // "Speed up" (Mods only): turns Kite Performance on and installs. Without a loader it makes
    // and opens a separate instance instead.
    function speedUp() {
        if (root.performance) {
            root.openBundles(true)
            return
        }
        if (!root.content.hasModLoader) {
            root.makePerformanceInstance()
            return
        }
        const watcher = root.shell ? root.shell.speedUp(root.instanceId) : null
        if (watcher)
            flows.track(watcher, "")
        else
            toast.show(qsTr("Kite Performance could not be set up for this instance."), "neutral")
    }
    function makePerformanceInstance() {
        const watcher = root.shell ? root.shell.createPerformanceInstance(root.instanceId) : null
        if (!watcher) {
            toast.show(qsTr("Kite Performance could not be set up for this instance."), "neutral")
            return
        }
        flows.track(watcher, "")
        watcher.finished.connect((ok) => { if (ok) root.openInstanceRequested(watcher.instanceId) })
    }
    // BundleDialog built on first use; only Add-ons need a loader.
    function openBundles(performance) {
        const open = () => {
            const loader = performance ? bundlePerformanceLoader : bundleAddonsLoader
            loader.active = true
            if (loader.item)
                loader.item.show()
        }
        if (performance)
            open()
        else
            root.withLoader(open)
    }
    // Review snapshots (InstancePage.review()): "modhealth", "bundles", "perfdialog".
    function review(name) {
        if (name === "modhealth")
            healthDialog.open()
        else if (name === "bundles")
            root.openBundles(false)
        else if (name === "perfdialog")
            root.openBundles(true)
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

        GridLayout {
            id: toolbar
            Layout.fillWidth: true
            columnSpacing: Theme.space.md
            rowSpacing: Theme.space.md
            columns: oneRow ? 2 : 1

            readonly property int searchMinWidth: 200
            readonly property bool showKinds: root.kinds.length > 1
            readonly property int kindsWidth: showKinds ? kindChips.implicitWidth + columnSpacing : 0
            readonly property int gap: Theme.space.sm
            readonly property int updatesIconWidth: root.updatable ? Theme.control.height + gap : 0
            readonly property int bundlesWidth: root.kind === "mods"
                                                ? (root.speedUpOffered ? speedUpMeasure.implicitWidth + gap : 0)
                                                  + addonsMeasure.implicitWidth + gap : 0
            readonly property int actionsWidth: folderMeasure.implicitWidth + moreButton.implicitWidth
                                                + addButton.implicitWidth + bundlesWidth + gap * 3
            // Measured against the tab's own width, which the toolbar's contents never feed back into.
            readonly property bool oneRow: root.width >= kindsWidth + searchMinWidth + updatesIconWidth + actionsWidth
            readonly property real actionsRowWidth: oneRow ? root.width - kindsWidth : root.width
            readonly property bool compact: actionsRowWidth < searchMinWidth + updatesIconWidth + actionsWidth
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

                // Only measured, so what they take does not change with what is shown.
                Button {
                    id: updatesMeasure
                    visible: false
                    text: qsTr("Check for updates")
                    icon.source: Icons.url("refresh")
                }
                Button {
                    id: folderMeasure
                    visible: false
                    text: qsTr("Open folder")
                    icon.source: Icons.url("folder")
                }
                SpeedUpButton {
                    id: speedUpMeasure
                    visible: false
                    on: root.performance
                }
                Button {
                    id: addonsMeasure
                    visible: false
                    text: qsTr("Add-ons")
                    icon.source: Icons.url("puzzle")
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
                    display: toolbar.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                    text: qsTr("Open folder")
                    icon.source: Icons.url("folder")
                    ToolTip.visible: hovered && display === AbstractButton.IconOnly
                    ToolTip.delay: 400
                    ToolTip.text: text
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
                        MenuEntry {
                            visible: root.kind === "mods" || root.kind === "coremods"
                            text: qsTr("Open configs folder")
                            icon.source: Icons.url("folder")
                            onTriggered: root.openFolderRequested(root.content ? root.content.configsDir : "")
                        }
                        MenuSeparator {}
                        MenuEntry {
                            visible: root.updatable
                            enabled: root.unlocked && root.count > 0 && !root.flowsBusy
                            text: qsTr("Check for updates")
                            icon.source: Icons.url("refresh")
                            onTriggered: root.withLoader(() => flows.checkUpdates([]))
                        }
                        MenuEntry {
                            visible: root.kind === "mods"
                            enabled: root.unlocked && root.count > 0 && !root.flowsBusy
                            text: qsTr("Verify dependencies")
                            icon.source: Icons.url("layers")
                            onTriggered: root.withLoader(() => flows.verifyDependencies())
                        }
                        MenuDivider {
                            visible: root.updatable || root.kind === "mods"
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

                HoverReason {
                    visible: root.kind === "mods" && root.speedUpOffered
                    reason: root.lockedReason
                    SpeedUpButton {
                        id: speedUpButton
                        on: root.performance
                        display: toolbar.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                        enabled: root.unlocked && !root.flowsBusy
                        onClicked: root.speedUp()
                    }
                }
                HoverReason {
                    visible: root.kind === "mods"
                    reason: root.lockedReason
                    Button {
                        id: addonsButton
                        enabled: root.unlocked && !root.flowsBusy
                        display: toolbar.compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
                        text: qsTr("Add-ons")
                        icon.source: Icons.url("puzzle")
                        ToolTip.visible: hovered && display === AbstractButton.IconOnly
                        ToolTip.delay: 400
                        ToolTip.text: text
                        onClicked: root.openBundles(false)
                    }
                }

                // Browsing works while the game runs; installing then waits.
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

        NoticeBar {
            Layout.fillWidth: true
            visible: root.flowsBusy
            tone: "info"
            text: flows.busyText
            progress: flows.busyProgress
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: root.kind === "mods" && !!root.health && root.health.count > 0
            text: !root.health ? ""
                  : root.health.blockingCount === 1 ? qsTr("1 problem may stop the game from starting.")
                  : root.health.blockingCount > 1
                    ? qsTr("%1 problems may stop the game from starting.").arg(root.health.blockingCount)
                  : root.health.count === 1 ? qsTr("1 warning about these mods.")
                  : qsTr("%1 warnings about these mods.").arg(root.health.count)
            actionText: qsTr("Show")
            onActionTriggered: healthDialog.open()
        }

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

    // The tab is reused across instances and a download outlives the switch: the notice stays with its instance.
    property string flowsInstanceId: ""
    readonly property bool flowsBusy: flows.busyText.length > 0 && root.flowsInstanceId === root.instanceId
    ContentUpdateFlows {
        id: flows
        content: root.content
        kind: root.kind
        noun: root.current.noun
        onNotify: (message, tone) => { if (message.length > 0) toast.show(message, tone) }
        onActivityChanged: root.flowsInstanceId = flows.activity ? root.instanceId : ""
    }

    Binding {
        target: root.listModel
        property: "filterText"
        value: searchField.text
        when: !!root.listModel
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.visible && packList.selecting && !searchField.activeFocus
                 && !exportDialog.visible && !bulkConfirm.visible && !healthDialog.visible
        onActivated: packList.clearSelection()
    }

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

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        width: parent.width
        // Not while something installs: its progress notice sits here.
        visible: root.isMinecraft && root.count === 0 && !root.flowsBusy
        title: root.current.emptyTitle
        body: root.current.emptyBody
        MeshIcon { iconName: root.current.icon; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        width: parent.width
        visible: !root.isMinecraft
        title: qsTr("No content")
        body: qsTr("This instance cannot have mods, resource packs or shader packs.")
        MeshIcon { iconName: "package"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
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
        // The new loader resolves in the background, so it may show up shortly after this closes.
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

    ModHealthDialog {
        id: healthDialog
        health: root.health
        unlocked: root.unlocked
        lockedReason: root.lockedReason
        onActionRequested: (action, key, dependencyId, replacesKey) => root.runHealthAction(action, key, dependencyId, replacesKey)
        onInstallAll: root.installMissing(root.health.installRows())
    }

    Component {
        id: bundleDialog
        BundleDialog {
            shell: root.shell
            instanceId: root.instanceId
            running: !!root.content && root.content.running
            bundles: root.shell ? root.shell.bundles : null
            updater: root.content ? root.content.updater : null
            onInstallRequested: (ids) => root.withLoader(() => flows.installBundle(ids))
        }
    }
    Loader {
        id: bundleAddonsLoader
        active: false
        sourceComponent: bundleDialog
        onLoaded: item.loader = Qt.binding(() => root.instanceLoader)
    }
    Loader {
        id: bundlePerformanceLoader
        active: false
        sourceComponent: bundleDialog
        onLoaded: {
            item.performance = true
            // The mode puts Fabric on a vanilla instance.
            item.loader = Qt.binding(() => root.instanceLoader.length > 0 ? root.instanceLoader : "fabric")
        }
    }

    onDetailsChanged: root.refreshPerformance()
    // The details object is reused across instances: a new instance arrives as a new id.
    onInstanceIdChanged: {
        root.refreshPerformance()
        root.trackRunningSetup()
    }
    onVisibleChanged: if (visible) {
        root.refreshPerformance()
        root.trackRunningSetup()
    }
    Component.onCompleted: {
        root.refreshPerformance()
        root.trackRunningSetup()
    }
    Connections {
        target: root.shell
        ignoreUnknownSignals: true
        function onPerformanceChanged(id) {
            if (id === root.instanceId)
                root.refreshPerformance()
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
