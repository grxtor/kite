// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance, opened from the library: the same banner the library's old
 * hero used -- what it is and what it runs, with its own actions behind one
 * ⋯ menu -- over tabs for what is inside it. Play/Stop no longer live on the
 * banner; the persistent play bar plays and stops whatever instance this
 * page has open instead. On a wide window the banner, tabs and tab content
 * stop at Theme.layout.maxContentWidth and centre.
 */
Item {
    id: root

    // Single-row model of this instance (the same roles as the library).
    property var headerModel: null
    // InstanceDetails for it.
    property var details: null
    property int systemMemoryMiB: 8192
    property string tab: "overview"
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

    function openHeaderMenu() {
        reviewDelay.run(function () {
            if (root.headerCard)
                instanceMenu.popup(root.headerCard, root.headerCard.width - instanceMenu.width, Theme.control.height)
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
    // Set from WorldsTab's per-row "Data packs" action, and read by
    // DataPacksTab's own selectedWorldRow - see both. Reset whenever a
    // different instance opens, so a row picked in a previous instance
    // never carries over onto one that may not even have that many worlds.
    property int pendingDataPackWorld: -1
    onDetailsChanged: root.pendingDataPackWorld = -1

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

            Button {
                flat: true
                text: qsTr("Library")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                // Pulled out by its padding and the empty third of the
                // chevron's square left of its stroke, so the ink starts on
                // the banner's and tabs' edge -- the same rule as Discover's
                // "Back to results".
                Layout.leftMargin: -(leftPadding + Math.round(Theme.icon.sm / 3))
                onClicked: root.backRequested()
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
                Layout.fillWidth: true
                Layout.maximumWidth: Theme.layout.maxContentWidth
                Layout.alignment: Qt.AlignHCenter
                // The group this instance lives in is more useful here than
                // a label that just repeats "you are looking at an
                // instance" -- and an ungrouped instance shows nothing.
                overline: group.length > 0 ? group : ""
                compact: true
                // Outside the Overview the tab's own content needs the room.
                slim: root.tab !== "overview"
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

        TabStrip {
            Layout.fillWidth: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            current: root.tab
            tabs: [
                { id: "overview", label: qsTr("Overview") }
            ].concat(root.managedPackModel ? [{ id: "managedpack", label: qsTr("Modpack") }] : [])
             .concat(root.details && root.details.isLegacy ? [] : [
                { id: "content", label: qsTr("Content"), count: root.modsModel ? contentTab.count : -1 },
                { id: "version", label: qsTr("Version") },
                { id: "browse", label: qsTr("Add content") }
             ])
             .concat([
                { id: "worlds", label: qsTr("Worlds"), count: root.worldsModel ? worldsTab.count : -1 }
             ])
             .concat(root.details && root.details.isMinecraft
                     ? [{ id: "datapacks", label: qsTr("Data packs") },
                        { id: "servers", label: qsTr("Servers") }] : [])
             .concat([
                { id: "backups", label: qsTr("Backups") },
                { id: "screenshots", label: qsTr("Screenshots"), count: root.shotsModel ? root.shotsModel.count : -1 },
                { id: "log", label: qsTr("Log") }
             ])
             .concat(root.details && root.details.isLegacy ? [] : [{ id: "settings", label: qsTr("Settings") }])
             .concat(root.details && root.details.isMinecraft
                     ? [{ id: "gameoptions", label: qsTr("Game options") }] : [])
             .concat(pluginPages.count === 0 ? []
                     : [{ id: "plugins", label: pluginPages.count === 1 && pluginPages.firstTitle.length > 0
                                                ? pluginPages.firstTitle : qsTr("Plugins") }])
            onActivated: (id) => root.tab = id
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: Theme.layout.maxContentWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.sm
            currentIndex: ["overview", "managedpack", "content", "version", "browse", "worlds", "datapacks", "servers", "backups", "screenshots", "log", "settings", "gameoptions", "plugins"].indexOf(root.tab)

            InstanceOverviewTab {
                details: root.details
                notes: root.details ? root.details.notes : ""
                onNotesEdited: (text) => { if (root.details) root.details.notes = text }
                onScreenshotsRequested: root.tab = "screenshots"
                onWorldsRequested: root.tab = "worlds"
                onManageContentRequested: root.tab = "content"
            }

            ManagedPackTab {
                controller: root.managedPackModel
                iconKey: root.headerCard ? root.headerCard.iconKey : ""
                onUpdated: root.backRequested()
            }

            ContentTab {
                id: contentTab
                details: root.details
                onOpenFolderRequested: (path) => root.openPathRequested(path)
                onBrowseRequested: root.tab = "browse"
            }

            VersionTab {
                details: root.details
            }

            ContentBrowserView {
                id: browseView
                browser: root.details ? root.details.contentBrowser : null
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
                model: root.shotsModel
                details: root.details
                directory: root.details ? root.details.screenshotsDir : ""
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            LogTab {
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
        // makes, found here by its subMenu.
        onAboutToShow: {
            for (var i = 0; i < instanceMenu.count; ++i) {
                var item = instanceMenu.itemAt(i)
                if (item && item.subMenu === playOptionsMenu) {
                    item.visible = instanceMenu.offerOptions
                    item.height = instanceMenu.offerOptions ? item.implicitHeight : 0
                    // An icon like its neighbours', or its label would
                    // start where theirs' icons do.
                    item.icon.source = Icons.url("sliders")
                }
            }
        }

        LaunchOptionsMenu {
            id: playOptionsMenu
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
            icon.source: Icons.url("package")
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
            icon.source: Icons.url("layers")
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
