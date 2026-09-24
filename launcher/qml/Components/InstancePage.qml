// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance, opened from the library: the same banner the library's old
 * hero used -- folder, what it runs, the classic editor -- over tabs for
 * what is inside it. Play/Stop no longer live on the banner; the persistent
 * play bar plays and stops whatever instance this page has open instead.
 */
Item {
    id: root

    // Single-row model of this instance (the same roles as the library).
    property var headerModel: null
    // InstanceDetails for it.
    property var details: null
    property int systemMemoryMiB: 8192
    property string tab: "overview"
    // (row, versionId) -> TaskWatcher for the content browser.
    property var contentInstaller: null
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
    signal classicEditorRequested(string id)
    signal openPathRequested(string path)
    // Bubbles up from the Servers tab's "Join" button - see Main.qml.
    signal joinServerRequested(string id, string address)

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
            // Theme.control.height, matching TopBar.titleRowHeight -- see
            // the topMargin comment above.
            Layout.preferredHeight: Theme.control.height
            spacing: Theme.space.sm

            Button {
                flat: true
                text: qsTr("Library")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
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
                required property string group
                Layout.fillWidth: true
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
                // classicEditorRequested is kept below only because Main.qml
                // still has a handler for it; nothing emits it any more.
                showEdit: false
                // Nothing under the QML shell answers this yet either.
                showMenu: false
                onFolderRequested: root.openPathRequested(root.details ? root.details.instanceRoot : "")

                // The overview needs these too, and this delegate is the
                // only place the header model's roles are at hand.
                Component.onCompleted: overview.syncFrom(this)
                onLastLaunchChanged: overview.syncFrom(this)
                onTotalTimePlayedChanged: overview.syncFrom(this)
                onGameVersionChanged: overview.syncFrom(this)
                onLoaderChanged: overview.syncFrom(this)
                onIsRunningChanged: overview.running = isRunning
            }
        }

        TabStrip {
            Layout.fillWidth: true
            current: root.tab
            tabs: [
                { id: "overview", label: qsTr("Overview") }
            ].concat(root.managedPackModel ? [{ id: "managedpack", label: qsTr("Modpack") }] : [])
             .concat([
                { id: "content", label: qsTr("Content"), count: root.modsModel ? contentTab.count : -1 },
                { id: "version", label: qsTr("Version") },
                { id: "browse", label: qsTr("Add content") },
                { id: "worlds", label: qsTr("Worlds"), count: root.worldsModel ? worldsTab.count : -1 }
             ])
             .concat(root.details && root.details.isMinecraft
                     ? [{ id: "datapacks", label: qsTr("Data packs") },
                        { id: "servers", label: qsTr("Servers") }] : [])
             .concat([
                { id: "backups", label: qsTr("Backups") },
                { id: "screenshots", label: qsTr("Screenshots"), count: root.shotsModel ? root.shotsModel.count : -1 },
                { id: "log", label: qsTr("Log") },
                { id: "settings", label: qsTr("Settings") }
             ])
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
            Layout.topMargin: Theme.space.sm
            currentIndex: ["overview", "managedpack", "content", "version", "browse", "worlds", "datapacks", "servers", "backups", "screenshots", "log", "settings", "gameoptions", "plugins"].indexOf(root.tab)

            InstanceOverviewTab {
                id: overview
                property bool running: false
                function syncFrom(card) {
                    loader = card.loader
                    gameVersion = card.gameVersion
                    lastLaunch = card.lastLaunch
                    totalTimePlayed = card.totalTimePlayed
                }
                details: root.details
                notes: root.details ? root.details.notes : ""
                onNotesEdited: (text) => { if (root.details) root.details.notes = text }
                onScreenshotsRequested: root.tab = "screenshots"
                onManageContentRequested: root.tab = "content"
            }

            ManagedPackTab {
                controller: root.managedPackModel
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
                browser: root.details ? root.details.contentBrowser : null
                installer: root.contentInstaller
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
                directory: root.details ? root.details.screenshotsDir : ""
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            LogTab {
                log: root.details ? root.details.log : null
                otherLogs: root.details ? root.details.otherLogs : null
                onOpenFolderRequested: (path) => root.openPathRequested(path)
            }

            InstanceSettingsTab {
                pluginSurfaces: root.settingsPlugins
                adapter: root.details ? root.details.settings : null
                systemMemoryMiB: root.systemMemoryMiB
                running: overview.running
            }

            GameOptionsTab {
                model: root.details ? root.details.gameOptions : null
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
}
