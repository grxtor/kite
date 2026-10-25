// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The Home page, Kite's own way in: the instance to play, big, with the
 * page's one primary Play (HomeHero, Lunar's single dominant launch); the
 * other instances to get back to under it ("Continue playing",
 * HomeContinuePlaying); then the whole Library, its toolbar and every
 * group -- all in one scrolling column. Beside them, on a wide enough
 * window, a side panel (HomeSidePanel): "Set up Kite" and Kite's news. The
 * default start page and the sidebar's first item. The play bar shows
 * here too (Main.qml's dockVisibleFor), so Play stays in reach once the
 * hero has scrolled away.
 *
 * The Continue playing rows' Plays are secondary (QuietPlayButton), and a
 * row's ⋯ opens the same instance menu as the library's cards.
 *
 * LIBRARY. LibrarySections, the very sections the Library page shows, under
 * a toolbar of its own (HomeLibraryToolbar): search, sort, grid or list,
 * New instance. The search filters `instanceModel` -- the shell's library
 * proxy, the one every section model follows -- and is put back on it
 * whenever this page is shown again, so the Library page's own search
 * never leaves Home filtered by words it does not show.
 *
 * Everything here comes from models the shell already keeps locally, so
 * opening this page makes no network call. The page builds no models
 * itself.
 */
Item {
    id: root

    // Most recently played instances first; never-played ones left out
    // (QmlShell.recentModel).
    property var recentModel: null
    // The hero's instance: a model whose one row is it (QmlShell.heroModel,
    // bound in Main.qml to the selection, else the instance played last,
    // else the first). Until it is set the hero shows recentModel's first
    // row.
    property var heroModel: null
    // The library, search-filtered (QmlShell's instance proxy).
    property var instanceModel: null
    // QmlShell.recentWorlds: the side panel's "Recent worlds", and the
    // hero's "Last world".
    // QmlShell.friends (FriendsController), or null.
    property var friends: null
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    property var recentWorldsModel: null
    // QmlShell.news (NewsModel): Kite's news, newest first, or null.
    property var news: null
    // Whether a Microsoft account is signed in: "Set up Kite"'s second
    // step.
    property bool msaSignedIn: false

    // -- The library sections' inputs; the same names as LibraryPage's ----
    // group -> that group's model (QmlShell.sectionModel). Without it the
    // library is one grid with no group headers.
    property var sectionModelFor: null
    property string selectedId: ""
    property var library: null
    property var launchOptionsFor: null
    property var exportControllerFor: null
    property var shortcutControllerFor: null

    // Play and Stop (the hero's Cancel too), from the hero and the rows.
    signal launchRequested(string id)
    signal stopRequested(string id)
    // Opens the instance page: the hero, a row, or a card's ⋯ > Open.
    signal openInstanceRequested(string id)
    // Opens the instance page on its Worlds tab: a recent world, or the
    // hero's last one.
    signal openInstanceWorldsRequested(string id)
    // New instance: the toolbar, "Set up Kite" and the empty page.
    signal createRequested()
    signal discoverRequested()
    // "Set up Kite"'s "Sign in with Microsoft".
    signal signInRequested()
    // The Friends page, on "friends" or "requests", or "add" (the add field
    // focused).
    signal friendsRequested(string tab)
    // Join a friend's server (a friend map).
    signal joinRequested(var friend)
    // A Continue playing row's ⋯ (or right-click). The page already opens
    // the library's instance menu for it; this only tells.
    signal menuRequested(string id, Item anchor)
    // The library's cards and their ⋯ menu, as LibraryPage's; the
    // launch-option ones come from the hero's ▾ too.
    signal selectRequested(string id)
    signal folderRequested(string id)
    signal renameRequested(string id, string name)
    signal iconRequested(string id, string iconKey)
    signal groupRequested(string id, string group)
    signal duplicateRequested(string id, string name, string group)
    signal deleteRequested(string id, string name)
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    // The library's own review routes (Main.qml's "export=", "shortcut=",
    // "librarymenu=", "groupmenu=" and "duplicate="), for when they point
    // here.
    function exportInstance(id, format) { sections.exportInstance(id, format) }
    function createShortcut(id) { sections.createShortcut(id) }
    function openInstanceMenu(id) { sections.openInstanceMenu(id, false, null, "", "") }
    function openGroupMenu(group) { sections.openGroupMenu(group) }
    function duplicateInstance(id) { sections.duplicateInstance(id) }
    function focusSearch() { toolbar.focusSearch() }

    // Kite's news as the side panel shows it: every post read off `news`,
    // the newest one still worth showing (or null), and whether there is
    // one.
    readonly property var newsEntries: side.newsEntries
    readonly property var featuredNews: side.featuredNews
    readonly property bool hasNews: side.hasNews

    // "grid" or "list"; session-only, like LibraryPage's.
    property string viewMode: "grid"
    readonly property string searchText: toolbar.searchText
    readonly property bool searching: root.searchText.length > 0

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int emptyStateWidth: 400
    readonly property int libraryCount: root.instanceModel && root.instanceModel.count !== undefined ? root.instanceModel.count : 0
    // A search that matches nothing still has a library behind it.
    readonly property bool hasLibrary: root.libraryCount > 0 || root.searching

    // Modrinth's side panel is about 300 wide; it shows only while the main
    // column keeps at least sideMinMainWidth beside it.
    readonly property int sideWidth: 300
    readonly property int sideMinMainWidth: 700
    readonly property bool showSide: side.hasContent
                                     && root.width - root.sideWidth - root.pagePadding * 2 >= root.sideMinMainWidth

    // The main column: the page left of the side panel, minus its padding,
    // never wider than Theme.layout.maxContentWidth, centred.
    readonly property int mainWidth: root.showSide ? root.width - root.sideWidth : root.width
    readonly property int contentWidth: Math.max(0, Math.min(root.mainWidth - root.pagePadding * 2, Theme.layout.maxContentWidth))
    readonly property int contentX: Math.round((root.mainWidth - root.contentWidth) / 2)

    function applySearch() {
        if (root.instanceModel && root.instanceModel.filterText !== undefined)
            root.instanceModel.filterText = toolbar.searchText
    }
    StackLayout.onIsCurrentItemChanged: if (StackLayout.isCurrentItem) root.applySearch()

    Flickable {
        id: flick
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.mainWidth
        visible: root.hasLibrary
        contentWidth: width
        contentHeight: content.y + content.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: !sections.dragging

        ScrollBar.vertical: ScrollBar {}

        // Clicking the empty page clears the selection, as on Library.
        MouseArea {
            width: flick.width
            height: Math.max(flick.height, flick.contentHeight)
            onClicked: root.selectRequested("")
        }

        Column {
            id: content
            x: root.contentX
            // The shell's panel adds no padding; the page pads itself.
            y: Theme.space.xl
            width: root.contentWidth
            spacing: Theme.space.xxl

            HomeHero {
                id: hero
                width: parent.width
                rowModel: root.heroModel ? root.heroModel : root.recentModel
                launchOptionsFor: root.launchOptionsFor
                worldsModel: root.recentWorldsModel
                onWorldsRequested: (id) => root.openInstanceWorldsRequested(id)
                onOpenRequested: (id) => root.openInstanceRequested(id)
                onLaunchRequested: (id) => root.launchRequested(id)
                onStopRequested: (id) => root.stopRequested(id)
                onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
                onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
                onProfilerChosen: (id, profilerKey) => root.profilerChosen(id, profilerKey)
                onPerformanceChosen: (id, enabled) => root.performanceChosen(id, enabled)
                onNavigateRequested: (target) => root.navigateRequested(target)
            }

            HomeContinuePlaying {
                width: parent.width
                recentModel: root.recentModel
                instanceModel: root.instanceModel
                excludeId: hero.instanceId
                frozen: root.searching
                onOpenRequested: (id) => root.openInstanceRequested(id)
                onLaunchRequested: (id) => root.launchRequested(id)
                onStopRequested: (id) => root.stopRequested(id)
                onMenuRequested: (id, running, name, iconKey, anchor) => {
                    sections.openInstanceMenu(id, running, anchor, name, iconKey)
                    root.menuRequested(id, anchor)
                }
            }

            // -- Library ----------------------------------------------------
            Column {
                width: parent.width
                spacing: Theme.space.lg

                Column {
                    width: parent.width
                    spacing: Theme.space.md

                    HomeSectionTitle {
                        width: parent.width
                        text: qsTr("Library")
                    }

                    HomeLibraryToolbar {
                        id: toolbar
                        width: parent.width
                        viewMode: root.viewMode
                        libraryModel: root.instanceModel
                        onViewModeActivated: (value) => root.viewMode = value
                        onNewInstanceRequested: root.createRequested()
                        onSearchTextChanged: root.applySearch()
                    }
                }

                LibrarySections {
                    id: sections
                    width: parent.width
                    height: implicitHeight
                    flickable: flick
                    instanceModel: root.instanceModel
                    sectionModelFor: root.sectionModelFor
                    searchText: root.searchText
                    selectedId: root.selectedId
                    viewMode: root.viewMode
                    launchOptionsFor: root.launchOptionsFor
                    exportControllerFor: root.exportControllerFor
                    shortcutControllerFor: root.shortcutControllerFor
                    library: root.library

                    onSelectRequested: (id) => root.selectRequested(id)
                    onLaunchRequested: (id) => root.launchRequested(id)
                    onStopRequested: (id) => root.stopRequested(id)
                    onEditRequested: (id) => root.openInstanceRequested(id)
                    onFolderRequested: (id) => root.folderRequested(id)
                    onRenameRequested: (id, name) => root.renameRequested(id, name)
                    onIconRequested: (id, iconKey) => root.iconRequested(id, iconKey)
                    onGroupRequested: (id, group) => root.groupRequested(id, group)
                    onDuplicateRequested: (id, name, group) => root.duplicateRequested(id, name, group)
                    onDeleteRequested: (id, name) => root.deleteRequested(id, name)
                    onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
                    onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
                    onProfilerChosen: (id, profilerKey) => root.profilerChosen(id, profilerKey)
                    onPerformanceChosen: (id, enabled) => root.performanceChosen(id, enabled)
                    onNavigateRequested: (target) => root.navigateRequested(target)
                }

                // A search that matched nothing, in place of the cards.
                Item {
                    visible: root.searching && root.libraryCount === 0
                    width: parent.width
                    height: noMatch.implicitHeight + Theme.space.xxl * 2

                    EmptyState {
                        id: noMatch
                        anchors.centerIn: parent
                        width: Math.min(parent.width, root.emptyStateWidth)
                        title: qsTr("No instances match “%1”").arg(root.searchText)
                        body: qsTr("Check the spelling, or search for part of the name.")
                        actionText: qsTr("Clear search")
                        actionIcon: "x"
                        onActionTriggered: toolbar.searchText = ""

                        MeshIcon {
                            iconName: "search"
                            size: Theme.icon.lg + Theme.space.md
                            color: Theme.palette.textTertiary
                        }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.mainWidth
        visible: root.hasLibrary
        flickable: flick
    }

    // Nothing in the library yet: the way in, in the middle of the page
    // (the side panel stays away until there is something to put in it).
    HomeEmptyState {
        x: Math.round((root.mainWidth - width) / 2)
        anchors.verticalCenter: parent.verticalCenter
        // A readable measure for the body text; EmptyState caps it.
        width: Math.min(root.mainWidth - root.pagePadding * 2, root.emptyStateWidth)
        visible: !root.hasLibrary
        onCreateRequested: root.createRequested()
        onDiscoverRequested: root.discoverRequested()
    }

    HomeSidePanel {
        id: side
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.sideWidth
        visible: root.showSide
        hasInstances: root.hasLibrary
        signedIn: root.msaSignedIn
        news: root.news
        recentWorldsModel: root.recentWorldsModel
        friends: root.friends
        joinFlow: root.joinFlow
        onFriendsRequested: (tab) => root.friendsRequested(tab)
        onNavigateRequested: (target) => root.navigateRequested(target)
        onJoinRequested: (friend) => root.joinRequested(friend)
        onCreateRequested: root.createRequested()
        onSignInRequested: root.signInRequested()
        onWorldRequested: (id) => root.openInstanceWorldsRequested(id)
    }
}
