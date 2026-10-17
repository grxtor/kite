// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance, opened from the library: a one-line banner -- what it is
 * and what it runs, with its own actions behind one ⋯ menu -- over tabs for
 * what is inside it. Play/Stop no longer live on the banner; the persistent
 * play bar plays and stops whatever instance this page has open instead. On
 * a wide window the banner, tabs and tab content stop at
 * Theme.layout.maxContentWidth and centre.
 *
 * Laid out the way Modrinth App and CurseForge lay out an instance: it opens
 * on its Content, and the tabs used every day (Content, Worlds, Screenshots,
 * Log, Settings) are always in view, with the rest under "More". Adding
 * content is an action of the Content tab ("Add content" in its toolbar),
 * not a tab of its own: like Modrinth App going to its browse page, it
 * opens the browser as a page of its own -- banner and tabs make way for
 * the results, which matters most in a small window -- and the back button
 * top left, now named after the instance, returns to the list.
 */
Item {
    id: root

    // Single-row model of this instance (the same roles as the library).
    property var headerModel: null
    // InstanceDetails for it.
    property var details: null
    property int systemMemoryMiB: 8192
    // The open tab, or "browse" for Content's "Add content" view. Empty, or
    // a tab this instance does not have, shows the first tab (Content, or
    // Worlds on an old-format instance).
    property string tab: ""
    // (anchor, instanceId) -> PluginSurfaceModel.
    property var pluginSurfacesFor: null
    // The account, for the top-right ProfileButton -- this page has no
    // TopBar of its own (see Main.qml), so it carries the same spot here.
    property string accountName: ""
    property string accountKind: ""
    property string accountAvatarSource: ""
    property var accountsController: null
    signal openAccountsRequested()
    readonly property string instanceId: details ? details.instanceId : ""
    readonly property var pagePlugins: pluginSurfacesFor && instanceId.length > 0 ? pluginSurfacesFor(1, instanceId) : null
    readonly property var settingsPlugins: pluginSurfacesFor && instanceId.length > 0 ? pluginSurfacesFor(2, instanceId) : null

    // Play/Stop no longer round-trip through this page: the persistent play
    // bar plays and stops whatever instance is open (see Main.qml).
    signal backRequested()
    signal openPathRequested(string path)
    // Bubbles up from the Servers tab's "Join" button - see Main.qml.
    signal joinServerRequested(string id, string address)

    // The same instance actions the library's ⋯ menu offers, with the same
    // arguments as LibraryPage's signals of these names, so Main.qml can
    // answer them with the same dialogs. Set `manageable` once it does:
    // until then the header menu offers only what this page can do itself.
    property bool manageable: false
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)

    // The header menu's Play options, with the same arguments as
    // LibraryPage's signals of these names (LaunchOptionsMenu.qml).
    property var launchOptionsFor: null
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal navigateRequested(string target)

    // id -> export / shortcut controller, as on LibraryPage: the header
    // menu offers "Export…" and "Create shortcut…" once these are set.
    property var exportControllerFor: null
    property var shortcutControllerFor: null
    // LibraryController, as on LibraryPage: Duplicate with options. Until
    // it is set, Duplicate goes to Main.qml's own dialog.
    property var library: null

    // The header ⋯ menu, open under the banner's top-right corner where
    // its button sits: for review snapshots (Main.qml's "instancemenu" dev
    // route). Deferred a beat, because the route opens the page in the same
    // breath and the banner has no width until it has been laid out.
    // The Content tab's "Export list" dialog and Add content's Filters
    // popup, open: for review snapshots (Main.qml's "exportlist" and
    // "browsefilters" dev routes, after instance=<id>). Deferred like the
    // header menu below, for the tab to be shown and laid out first.
    function openContentExportList() {
        root.tab = "content"
        reviewDelay.run(function () { contentTab.exportList([]) })
    }
    function openBrowseFilters() {
        root.tab = "browse"
        reviewDelay.run(browseView.openFilters)
    }
    // Content's "Add content": the browser, on the kind Content showed.
    function browseContent(kind) {
        browseView.showKind(kind)
        root.tab = "browse"
    }
    // Back from the browser to Content, on the kind just browsed.
    function leaveBrowse() {
        contentTab.showKind(browseView.browser ? browseView.browser.contentType : "")
        root.tab = "content"
    }

    // A tab's own state for review snapshots (Main.qml's "review=<name>"
    // dev route, after instance=<id>): the tab is shown first, then asked.
    function review(name) {
        const owners = {
            versionmore: ["version", versionTab], newcomponent: ["version", versionTab],
            otherlogs: ["log", logTab], logfind: ["log", logTab],
            shotselect: ["screenshots", shotsTab], shotrename: ["screenshots", shotsTab],
            browsepick: ["browse", browseView]
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

    // The header delegate below, for the menu's name/icon/group.
    property var headerCard: null
    // Whether the opened instance's game is running (from the header model).
    property bool running: false

    readonly property var modsModel: details ? details.mods : null
    readonly property var worldsModel: details ? details.worlds : null
    readonly property var shotsModel: details ? details.screenshots : null
    readonly property var managedPackModel: details ? details.managedPack : null
    readonly property bool isLegacy: !!details && details.isLegacy
    readonly property bool isMinecraft: !!details && details.isMinecraft

    // Everyday tabs first, in view at every width; `more` ones always sit in
    // the strip's More menu (see TabStrip). An old-format instance can only
    // be upgraded, so it has no Content, Version or Settings.
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
    // What is on screen: `tab` when this instance has it, else the first tab.
    readonly property string shownTab: {
        if (root.tab === "browse" && !root.isLegacy)
            return "browse"
        const ids = root.tabList.map((t) => t.id)
        return ids.indexOf(root.tab) >= 0 ? root.tab : (ids.length > 0 ? ids[0] : "")
    }
    readonly property bool browsing: root.shownTab === "browse"
    // Set from WorldsTab's per-row "Data packs" action, and read by
    // DataPacksTab's own selectedWorldRow - see both. Reset whenever a
    // different instance opens, so a row picked in a previous instance
    // never carries over onto one that may not even have that many worlds.
    property int pendingDataPackWorld: -1
    // A different instance also opens on its first tab, as in any launcher.
    onDetailsChanged: {
        root.pendingDataPackWorld = -1
        root.tab = ""
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.xl + Theme.space.xs
        // Theme.space.sm, matching TopBar's own title-row top margin exactly
        // (see TopBar.qml): this page has no TopBar of its own, but the
        // account still has to land in the same pixel spot switching to and
        // from a page that does.
        anchors.topMargin: Theme.space.sm
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            // Capped and centred like the banner and tabs below, so on a wide
            // window the account lands where every other page puts it.
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            // Theme.control.height, matching TopBar.titleRowHeight -- see
            // the topMargin comment above.
            Layout.preferredHeight: Theme.control.height
            spacing: Theme.space.sm

            // Back to the library -- or, from Add content, back to this
            // instance's Content.
            Button {
                flat: true
                text: root.browsing && root.headerCard ? root.headerCard.name : qsTr("Library")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                // Pulled out by its padding and the empty third of the
                // chevron's square left of its stroke, so the ink starts on
                // the banner's and tabs' edge -- the same rule as Discover's
                // "Back to results".
                Layout.leftMargin: -(leftPadding + Math.round(Theme.icon.sm / 3))
                // A long instance name gives way before the account does.
                Layout.maximumWidth: Math.round(root.width / 2)
                onClicked: root.browsing ? root.leaveBrowse() : root.backRequested()
            }

            Item { Layout.fillWidth: true }

            // No TopBar reaches this page (see Main.qml) -- the account
            // still needs the same consistent top-right spot every other
            // page gives it.
            ProfileButton {
                name: root.accountName
                kind: root.accountKind
                avatarSource: root.accountAvatarSource
                controller: root.accountsController
                onOpenAccountsRequested: root.openAccountsRequested()
            }
        }

        Repeater {
            model: root.headerModel
            delegate: ContinueCard {
                id: headerCard
                required property string group
                // The row itself, for the role ContinueCard does not
                // require: the pack's own art for the backdrop.
                required property var model
                packArt: headerCard.model.packArt || ""
                Layout.fillWidth: true
                Layout.maximumWidth: Theme.layout.maxContentWidth
                Layout.alignment: Qt.AlignHCenter
                // One line -- icon, name, tags, ⋯ -- like Modrinth App's
                // instance header: every tab's own content needs the room,
                // most of all at the smallest window.
                compact: true
                slim: true
                // Add content is a page of its own (see the file comment).
                visible: !root.browsing
                // The persistent play bar plays/stops the opened instance
                // now; this header no longer needs its own Play/Stop too.
                showPlay: false
                // The "Classic editor" button that used to sit here opened
                // a widget InstanceWindow, which the QML shell must never
                // do -- this page's own tabs (Servers, Backups, Worlds,
                // Managed pack, ...) now cover everything it did instead.
                showEdit: false
                // Folder and the rest live together in one ⋯ menu.
                showFolder: false
                menu: instanceMenu
                Component.onDestruction: if (root.headerCard === headerCard) root.headerCard = null

                // The settings tab needs to know whether the game runs, and
                // this delegate is the only place the header model's roles
                // are at hand.
                Component.onCompleted: {
                    root.headerCard = headerCard
                    root.running = isRunning
                }
                onIsRunningChanged: root.running = isRunning
            }
        }

        // An old-format instance: the one thing to do with it, above
        // whichever tab is open.
        LegacyUpgradeNotice {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            details: root.details
        }

        TabStrip {
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
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onBrowseRequested: (kind) => root.browseContent(kind)
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
                onOpenFolderRequested: (path) => root.openPathRequested(path)
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
                    // A single plugin page is already named by its tab.
                    showTitles: count > 1
                }
            }
        }
    }

    // The header's ⋯ menu: Play options and the folders first, then --
    // once Main.qml answers them (see `manageable` and the controller
    // functions above) -- the library's own instance actions.
    Menu {
        id: instanceMenu
        readonly property bool isMinecraft: !!root.details && root.details.isMinecraft
        readonly property string name: root.headerCard ? root.headerCard.name : ""
        readonly property string iconKey: root.headerCard ? root.headerCard.iconKey : ""
        readonly property string group: root.headerCard ? root.headerCard.group : ""
        // Play options make no sense while the game runs.
        readonly property bool offerOptions: !root.running && !!root.launchOptionsFor
        readonly property bool offerManage: root.manageable || exportFlows.canExport || exportFlows.canCreateShortcut

        // A submenu has no `visible` of its own: its entry is a MenuItem Qt
        // makes, found here by its subMenu. Kept in step ahead of opening:
        // the ⋯ button places the menu by its width before aboutToShow, so
        // an entry that appeared only then pushed the menu past the button.
        function syncOptionsEntry() {
            for (var i = 0; i < instanceMenu.count; ++i) {
                var item = instanceMenu.itemAt(i)
                if (item && item.subMenu === playOptionsMenu) {
                    item.visible = instanceMenu.offerOptions
                    item.height = instanceMenu.offerOptions ? item.implicitHeight : 0
                }
            }
        }
        onOfferOptionsChanged: syncOptionsEntry()
        Component.onCompleted: syncOptionsEntry()

        LaunchOptionsMenu {
            id: playOptionsMenu
            // An icon like its neighbours', or its label would start where
            // theirs' icons do.
            icon.source: Icons.url("sliders")
            instanceId: root.instanceId
            optionsFor: root.launchOptionsFor
            onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
            onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
            onProfilerChosen: (id, key) => root.profilerChosen(id, key)
            onNavigateRequested: (target) => root.navigateRequested(target)
        }
        MenuSeparator {
            visible: instanceMenu.offerOptions
            height: visible ? implicitHeight : 0
        }
        MenuItem {
            text: qsTr("Open folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.instanceRoot : "")
        }
        MenuItem {
            // A hidden item still takes its row in a Menu; zero it as well.
            visible: instanceMenu.isMinecraft && root.details.gameRoot !== root.details.instanceRoot
            height: visible ? implicitHeight : 0
            text: qsTr("Open .minecraft folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.gameRoot : "")
        }
        MenuItem {
            // A hidden item still takes its row in a Menu; zero it as well.
            visible: instanceMenu.isMinecraft
            height: visible ? implicitHeight : 0
            text: qsTr("Open mods folder")
            icon.source: Icons.url("folder")
            onTriggered: root.openPathRequested(root.details ? root.details.modsDir : "")
        }
        MenuSeparator {
            visible: instanceMenu.offerManage
            height: visible ? implicitHeight : 0
        }
        MenuItem {
            visible: root.manageable
            height: visible ? implicitHeight : 0
            text: qsTr("Rename…")
            icon.source: Icons.url("edit")
            onTriggered: root.renameRequested(root.instanceId, instanceMenu.name)
        }
        MenuItem {
            visible: root.manageable
            height: visible ? implicitHeight : 0
            text: qsTr("Change icon…")
            icon.source: Icons.url("image")
            onTriggered: root.iconRequested(root.instanceId, instanceMenu.iconKey)
        }
        MenuItem {
            visible: root.manageable
            height: visible ? implicitHeight : 0
            text: qsTr("Change group…")
            icon.source: Icons.url("folder-input")
            onTriggered: root.groupRequested(root.instanceId, instanceMenu.group)
        }
        MenuItem {
            visible: root.manageable
            height: visible ? implicitHeight : 0
            text: qsTr("Duplicate…")
            icon.source: Icons.url("copy")
            onTriggered: exportFlows.canDuplicate
                         ? exportFlows.duplicateInstance(root.instanceId, instanceMenu.name, instanceMenu.group, instanceMenu.iconKey)
                         : root.duplicateRequested(root.instanceId, instanceMenu.name, instanceMenu.group)
        }
        MenuItem {
            visible: exportFlows.canExport
            height: visible ? implicitHeight : 0
            text: qsTr("Export…")
            icon.source: Icons.url("archive")
            onTriggered: exportFlows.exportInstance(root.instanceId)
        }
        MenuItem {
            visible: exportFlows.canCreateShortcut
            height: visible ? implicitHeight : 0
            text: qsTr("Create shortcut…")
            icon.source: Icons.url("external-link")
            onTriggered: exportFlows.createShortcut(root.instanceId)
        }
        MenuSeparator {
            visible: root.manageable
            height: visible ? implicitHeight : 0
        }
        MenuItem {
            visible: root.manageable
            height: visible ? implicitHeight : 0
            text: qsTr("Delete…")
            icon.source: Icons.url("trash")
            onTriggered: root.deleteRequested(root.instanceId, instanceMenu.name)
        }
    }

    // "Export…" and "Create shortcut…": the dialogs and their toast, shared
    // with LibraryPage.
    InstanceExportFlows {
        id: exportFlows
        exportControllerFor: root.exportControllerFor
        shortcutControllerFor: root.shortcutControllerFor
        library: root.library
    }
}
