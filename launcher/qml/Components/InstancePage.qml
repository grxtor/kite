// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/* One instance: a header (icon, name, Play, ⋯ menu) over tabs; opens on Content. */
Item {
    id: root

    // Single-row model of this instance (the same roles as the library).
    property var headerModel: null
    property var details: null
    property int systemMemoryMiB: 8192
    // The open tab, or "browse" for Content's "Add content" view; empty or unknown shows the first tab.
    property string tab: ""
    property var shell: null
    // (anchor, instanceId) -> PluginSurfaceModel.
    property var pluginSurfacesFor: null
    // The account for the top-right ProfileButton; this page has no TopBar.
    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""
    property var accountsController: null
    signal openAccountsRequested()
    readonly property string instanceId: details ? details.instanceId : ""
    readonly property var pagePlugins: pluginSurfacesFor && instanceId.length > 0 ? pluginSurfacesFor(1, instanceId) : null
    readonly property var settingsPlugins: pluginSurfacesFor && instanceId.length > 0 ? pluginSurfacesFor(2, instanceId) : null

    // For a drop on this page: asks for a mod loader first when there is none. True when it took over, to
    // run @p action once a loader is in.
    function needsLoaderFirst(action) {
        return contentTab.needsLoader(action)
    }

    signal backRequested()
    // Open another instance (the Mods tab's new Kite Performance one).
    signal openInstanceRequested(string id)
    signal playRequested(string id)
    signal stopRequested(string id)
    signal openPathRequested(string path)
    // From the Servers tab's "Join" button.
    signal joinServerRequested(string id, string address)

    // The library's instance actions; set `manageable` once Main.qml answers them, until then the
    // header menu offers only what this page can do.
    property bool manageable: false
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)

    property var launchOptionsFor: null
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    // id -> export / shortcut controller; the header menu offers "Export…" and "Create shortcut…"
    // once set.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    // LibraryController: Duplicate with options; without it, Duplicate goes to Main.qml's own dialog.
    property var library: null

    // Dev-route review snapshots ("instancemenu", "exportlist", "browsefilters"): open after a
    // beat, once the page is laid out.
    function openContentExportList() {
        root.tab = "content"
        reviewDelay.run(function () { contentTab.exportList([]) })
    }
    function openBrowseFilters() {
        root.tab = "browse"
        reviewDelay.run(browseView.openFilters)
    }
    function browseContent(kind) {
        browseView.showKind(kind)
        root.tab = "browse"
    }
    function leaveBrowse() {
        contentTab.showKind(browseView.browser ? browseView.browser.contentType : "")
        root.tab = "content"
    }

    // A tab's own state for review snapshots (dev route "review=<name>").
    function review(name) {
        const owners = {
            versionmore: ["version", versionTab], newcomponent: ["version", versionTab],
            otherlogs: ["log", logTab], logfind: ["log", logTab],
            shotselect: ["screenshots", shotsTab], shotrename: ["screenshots", shotsTab],
            browsepick: ["browse", browseView], modhealth: ["content", contentTab], bundles: ["content", contentTab], perfdialog: ["content", contentTab]
        }
        const owner = owners[name]
        if (!owner)
            return
        root.tab = owner[0]
        reviewDelay.run(function () { owner[1].review(name) })
    }

    function openHeaderMenu() {
        reviewDelay.run(function () {
            if (root.headerCard)
                root.headerCard.openMenu()
        })
    }
    Timer {
        id: reviewDelay
        property var action: null
        function run(fn) {
            action = fn
            restart()
        }
        interval: Theme.motion.normal
        onTriggered: if (action) action()
    }

    property var headerCard: null
    // File name of the screenshot the instance's own cover was made from; "" when it has none.
    readonly property string coverSource: root.headerCard ? root.headerCard.coverSource : ""
    readonly property int headerIconSize: 64
    readonly property int headerActionSize: Theme.control.heightLg
    property bool running: false

    readonly property var modsModel: details ? details.mods : null
    readonly property var worldsModel: details ? details.worlds : null
    readonly property var shotsModel: details ? details.screenshots : null
    readonly property var managedPackModel: details ? details.managedPack : null
    readonly property bool isLegacy: !!details && details.isLegacy
    readonly property bool isMinecraft: !!details && details.isMinecraft

    // Everyday tabs first; `more` ones sit in the strip's More menu. An old-format instance can
    // only be upgraded.
    readonly property var tabList: {
        const tabs = []
        if (!root.isLegacy)
            tabs.push({ id: "content", label: qsTr("Content"), count: root.modsModel ? contentTab.count : -1 })
        tabs.push({ id: "worlds", label: qsTr("Worlds"), count: root.worldsModel ? worldsTab.count : -1 })
        tabs.push({ id: "screenshots", label: qsTr("Screenshots"), count: root.shotsModel ? root.shotsModel.count : -1 })
        tabs.push({ id: "log", label: qsTr("Log") })
        if (!root.isLegacy)
            tabs.push({ id: "settings", label: qsTr("Settings") })
        if (root.managedPackModel)
            tabs.push({ id: "managedpack", label: qsTr("Modpack"), more: true })
        if (!root.isLegacy)
            tabs.push({ id: "version", label: qsTr("Version"), more: true })
        if (root.isMinecraft) {
            tabs.push({ id: "datapacks", label: qsTr("Data packs"), more: true })
            tabs.push({ id: "servers", label: qsTr("Servers"), more: true })
        }
        tabs.push({ id: "backups", label: qsTr("Backups"), more: true })
        if (root.isMinecraft)
            tabs.push({ id: "gameoptions", label: qsTr("Game options"), more: true })
        tabs.push({ id: "notes", label: qsTr("Notes"), more: true })
        if (pluginPages.count > 0)
            tabs.push({ id: "plugins", more: true,
                        label: pluginPages.count === 1 && pluginPages.firstTitle.length > 0
                               ? pluginPages.firstTitle : qsTr("Plugins") })
        return tabs
    }
    readonly property string shownTab: {
        if (root.tab === "browse" && !root.isLegacy)
            return "browse"
        const ids = root.tabList.map((t) => t.id)
        return ids.indexOf(root.tab) >= 0 ? root.tab : (ids.length > 0 ? ids[0] : "")
    }
    readonly property bool browsing: root.shownTab === "browse"
    // Set by WorldsTab's "Data packs" action and read by DataPacksTab; reset when another instance
    // opens.
    property int pendingDataPackWorld: -1
    onDetailsChanged: {
        root.pendingDataPackWorld = -1
        root.tab = ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        // The shell's inset panel adds no padding.
        anchors.topMargin: Theme.space.xl
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.md

        Repeater {
            model: root.headerModel
            delegate: Item {
                id: headerCard
                required property var model
                readonly property string name: model.name || ""
                readonly property string iconKey: model.iconKey || ""
                readonly property string group: model.group || ""
                readonly property string coverSource: model.coverSource || ""
                readonly property bool isRunning: !!model.isRunning
                readonly property string launchStatus: model.launchStatus || ""
                readonly property real launchProgress: model.launchProgress === undefined ? -1 : model.launchProgress
                readonly property bool launching: launchStatus.length > 0
                // Hidden while "Show play time per instance" is off.
                readonly property string played: SettingsStore.bool("ShowGameTime")
                                                 ? Format.playTime(model.totalTimePlayed) : ""

                Layout.fillWidth: true
                Layout.maximumWidth: Theme.layout.maxContentWidth
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.space.xs
                implicitHeight: headerRow.implicitHeight
                visible: !root.browsing
                Component.onDestruction: if (root.headerCard === headerCard) root.headerCard = null

                // Right-aligned under the ⋯ button and held there: entries that show or hide as the
                // menu opens change its width.
                function openMenu() {
                    if (!moreButton)
                        return
                    instanceMenu.popup(moreButton, moreButton.width - instanceMenu.width,
                                       moreButton.height + Theme.space.xs)
                    instanceMenu.x = Qt.binding(() => moreButton.width - instanceMenu.width)
                }

                // The only place the header model's roles are at hand.
                Component.onCompleted: {
                    root.headerCard = headerCard
                    root.running = isRunning
                }
                onIsRunningChanged: root.running = isRunning

                component MetaItem: Row {
                    property string iconName
                    property string text
                    spacing: Theme.space.xs
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: parent.iconName
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.text
                        role: "label"
                        tone: "secondary"
                        font.weight: Font.Medium
                    }
                }

                RowLayout {
                    id: headerRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: Theme.space.lg

                    HomeHeroTile {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: root.headerIconSize
                        Layout.preferredHeight: root.headerIconSize
                        iconKey: headerCard.iconKey.length > 0 ? headerCard.iconKey : "default"
                        instanceId: root.instanceId
                        packArt: headerCard.model.packArt || ""
                        matte: Theme.palette.canvas
                        radius: Theme.radius.lg
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.space.xs

                        Copy {
                            Layout.fillWidth: true
                            text: headerCard.name
                            elide: Text.ElideRight
                            role: "display"
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Theme.space.xs
                            MetaItem {
                                text: headerCard.model.loader.length > 0 ? headerCard.model.loader : qsTr("Vanilla")
                                iconName: "layers"
                                rightPadding: Theme.space.md
                            }
                            MetaItem {
                                visible: headerCard.model.gameVersion.length > 0
                                text: headerCard.model.gameVersion
                                iconName: "cube"
                                rightPadding: Theme.space.md
                            }
                            // Clock = last played, hourglass = total play time; the ▶ triangle only
                            // means Play.
                            MetaItem {
                                text: Format.lastPlayed(headerCard.model.lastLaunch)
                                iconName: "clock"
                                rightPadding: Theme.space.md
                            }
                            MetaItem {
                                visible: headerCard.played.length > 0
                                text: qsTr("%1 played").arg(headerCard.played)
                                iconName: "hourglass"
                            }
                        }

                        ColumnLayout {
                            visible: headerCard.launching
                            Layout.preferredWidth: 320
                            spacing: Theme.space.xs
                            Copy {
                                Layout.fillWidth: true
                                text: headerCard.launchProgress >= 0
                                      ? qsTr("%1, %2%").arg(headerCard.launchStatus).arg(Math.round(headerCard.launchProgress * 100))
                                      : headerCard.launchStatus
                                elide: Text.ElideRight
                                role: "label"
                                tone: "secondary"
                            }
                            LaunchProgressBar {
                                Layout.fillWidth: true
                                progress: headerCard.launchProgress
                            }
                        }
                    }

                    Row {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.space.sm

                        PlayButton {
                            round: false
                            size: root.headerActionSize
                            running: headerCard.isRunning
                            busy: headerCard.launching
                            enabled: !headerCard.launching && (headerCard.isRunning || !!headerCard.model.canLaunch)
                            onClicked: headerCard.isRunning ? root.stopRequested(root.instanceId)
                                                             : root.playRequested(root.instanceId)
                        }
                        IconButton {
                            id: moreButton
                            size: root.headerActionSize
                            iconName: "more"
                            tip: qsTr("More")
                            Accessible.role: Accessible.ButtonMenu
                            onClicked: instanceMenu.visible ? instanceMenu.close() : headerCard.openMenu()
                        }
                    }
                }
            }
        }

        LegacyUpgradeNotice {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            details: root.details
        }

        TabStrip {
            Layout.topMargin: Theme.space.xs
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            visible: !root.browsing
            current: root.shownTab
            tabs: root.tabList
            onActivated: (id) => root.tab = id
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.sm
            currentIndex: ["content", "browse", "managedpack", "version", "worlds", "datapacks", "servers", "backups", "screenshots", "log", "settings", "gameoptions", "notes", "plugins"].indexOf(root.shownTab)

            ContentTab {
                id: contentTab
                details: root.details
                shell: root.shell
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onBrowseRequested: (kind) => root.browseContent(kind)
                onOpenInstanceRequested: (id) => root.openInstanceRequested(id)
            }

            ContentBrowserView {
                id: browseView
                browser: root.details ? root.details.contentBrowser : null
                details: root.details
                onBackRequested: root.leaveBrowse()
            }

            ManagedPackTab {
                controller: root.managedPackModel
                onUpdated: root.backRequested()
            }

            VersionTab {
                id: versionTab
                details: root.details
            }

            WorldsTab {
                id: worldsTab
                details: root.details
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onDataPacksRequested: (row) => {
                    root.pendingDataPackWorld = row
                    root.tab = "datapacks"
                }
            }

            DataPacksTab {
                details: root.details
                selectedWorldRow: root.pendingDataPackWorld
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            ServersTab {
                model: root.details ? root.details.servers : null
                serversDir: root.details ? root.details.serversDir : ""
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onJoinRequested: (address) => root.joinServerRequested(root.instanceId, address)
            }

            BackupsTab {
                controller: root.details ? root.details.backups : null
            }

            ScreenshotsTab {
                id: shotsTab
                model: root.shotsModel
                details: root.details
                directory: root.details ? root.details.screenshotsDir : ""
                coverSource: root.coverSource
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onCoverRequested: (path) => { if (root.shell) root.shell.setInstanceCover(root.instanceId, path) }
                onCoverCleared: { if (root.shell) root.shell.clearInstanceCover(root.instanceId) }
                onCoverRenamed: (path) => { if (root.shell) root.shell.renameInstanceCoverSource(root.instanceId, path) }
            }

            LogTab {
                id: logTab
                log: root.details ? root.details.log : null
                otherLogs: root.details ? root.details.otherLogs : null
                details: root.details
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            InstanceSettingsTab {
                pluginSurfaces: root.settingsPlugins
                adapter: root.details ? root.details.settings : null
                systemMemoryMiB: root.systemMemoryMiB
                running: root.running
                loader: root.details && root.details.contentBrowser ? root.details.contentBrowser.instanceLoader : ""
                // Read again when the game stops (the manifest is fetched at launch) and when the
                // tab is shown (the loader may have changed in Version).
                kiteModSupport: {
                    root.running
                    visible
                    return root.details ? root.details.kiteModSupport() : "unknown"
                }
            }

            GameOptionsTab {
                model: root.details ? root.details.gameOptions : null
                running: root.running
            }

            InstanceNotesTab {
                details: root.details
            }

            SettingsScroll {
                title: qsTr("Plugins")
                PluginSurfaces {
                    id: pluginPages
                    width: parent.width
                    model: root.pagePlugins
                    showTitles: count > 1
                }
            }
        }
    }

    Menu {
        id: instanceMenu
        readonly property bool isMinecraft: !!root.details && root.details.isMinecraft
        readonly property string name: root.headerCard ? root.headerCard.name : ""
        readonly property string iconKey: root.headerCard ? root.headerCard.iconKey : ""
        readonly property string group: root.headerCard ? root.headerCard.group : ""
        readonly property bool offerOptions: !root.running && !!root.launchOptionsFor
        readonly property bool offerManage: root.manageable || exportFlows.canExport || exportFlows.canCreateShortcut

        SubmenuEntry {
            menu: instanceMenu
            submenu: playOptionsMenu
            shown: instanceMenu.offerOptions
        }
        LaunchOptionsMenu {
            id: playOptionsMenu
            // An icon like its neighbours', so the label lines up.
            icon.source: Icons.url("sliders")
            instanceId: root.instanceId
            optionsFor: root.launchOptionsFor
            onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
            onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
            onProfilerChosen: (id, key) => root.profilerChosen(id, key)
            onPerformanceChosen: (id, enabled) => root.performanceChosen(id, enabled)
            onNavigateRequested: (target) => root.navigateRequested(target)
        }
        MenuDivider {
            visible: instanceMenu.offerOptions
        }
        MenuItem {
            text: qsTr("Open folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.instanceRoot : "")
        }
        MenuEntry {
            visible: instanceMenu.isMinecraft && root.details.gameRoot !== root.details.instanceRoot
            text: qsTr("Open .minecraft folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.gameRoot : "")
        }
        MenuEntry {
            visible: instanceMenu.isMinecraft
            text: qsTr("Open mods folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.modsDir : "")
        }
        MenuDivider {
            visible: instanceMenu.offerManage
        }
        MenuEntry {
            visible: root.manageable
            text: qsTr("Rename…")
            icon.source: Icons.url("edit")
            onTriggered: root.renameRequested(root.instanceId, instanceMenu.name)
        }
        MenuEntry {
            visible: root.manageable
            text: qsTr("Change icon…")
            icon.source: Icons.url("image")
            onTriggered: root.iconRequested(root.instanceId, instanceMenu.iconKey)
        }
        MenuEntry {
            visible: root.manageable
            text: qsTr("Change group…")
            icon.source: Icons.url("folder-input")
            onTriggered: root.groupRequested(root.instanceId, instanceMenu.group)
        }
        MenuEntry {
            visible: root.manageable
            text: qsTr("Duplicate…")
            icon.source: Icons.url("copy")
            onTriggered: exportFlows.canDuplicate
                         ? exportFlows.duplicateInstance(root.instanceId, instanceMenu.name, instanceMenu.group, instanceMenu.iconKey)
                         : root.duplicateRequested(root.instanceId, instanceMenu.name, instanceMenu.group)
        }
        MenuEntry {
            visible: exportFlows.canExport
            text: qsTr("Export…")
            icon.source: Icons.url("archive")
            onTriggered: exportFlows.exportInstance(root.instanceId)
        }
        MenuEntry {
            visible: exportFlows.canCreateShortcut
            text: qsTr("Create shortcut…")
            icon.source: Icons.url("external-link")
            onTriggered: exportFlows.createShortcut(root.instanceId)
        }
        MenuDivider {
            visible: root.manageable
        }
        MenuEntry {
            visible: root.manageable
            text: qsTr("Delete…")
            icon.source: Icons.url("trash")
            // A running game's folder is in use.
            enabled: !root.running
            onTriggered: root.deleteRequested(root.instanceId, instanceMenu.name)
        }
    }

    InstanceExportFlows {
        id: exportFlows
        exportControllerFor: root.exportControllerFor
        shortcutControllerFor: root.shortcutControllerFor
        library: root.library
    }
}
