// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The Home page: hero, "Continue playing" and the library in one scrolling column, with a side panel on wide windows.
 * Routes Theme.look ("classic", "stage", "calm", "night") to the pieces of each look; builds no models itself.
 */
Item {
    id: root

    // Most recently played first; never-played instances left out.
    property var recentModel: null
    // A model whose one row is the hero's instance; recentModel's first row until it is set.
    property var heroModel: null
    property var instanceModel: null
    // FriendsController, or null.
    property var friends: null
    // Main.qml's FriendPlaces.
    property FriendPlaces friendPlaces: FriendPlaces {}
    property var joinFlow: null
    // The friends panel on the right edge is out; the side panel leaves the friends to it.
    property bool friendsInPanel: false
    property var recentWorldsModel: null
    property var news: null
    property bool msaSignedIn: false
    // QmlShell.launcherImport, or null.
    property var launcherImport: null

    // group -> that group's model; without it the library is one grid with no headers.
    property var sectionModelFor: null
    property string selectedId: ""
    property var library: null
    property var launchOptionsFor: null
    property var exportControllerFor: null
    property var shareCodes: null
    property var shortcutControllerFor: null

    signal launchRequested(string id)
    signal stopRequested(string id)
    signal openInstanceRequested(string id)
    signal openInstanceWorldsRequested(string id)
    signal createRequested()
    signal discoverRequested()
    signal signInRequested()
    // "Set up Kite" offers the instances other launchers have: New instance on its "From another launcher" tab.
    signal importLaunchersRequested()
    // tab: "friends", "requests" or "add".
    signal friendsRequested(string tab)
    signal joinRequested(var friend)
    // A Continue playing row's ⋯; the page already opens the instance menu, this only tells.
    signal menuRequested(string id, Item anchor)
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

    // Review routes, forwarded to the library sections.
    function exportInstance(id, format) { sections.exportInstance(id, format) }
    function createShortcut(id) { sections.createShortcut(id) }
    function openInstanceMenu(id) { sections.openInstanceMenu(id, false, null, "", "") }
    function openGroupMenu(group) { sections.openGroupMenu(group) }
    function duplicateInstance(id) { sections.duplicateInstance(id) }
    function shareInstance(id, name) { sections.shareInstance(id, name) }
    function focusSearch() { toolbar.focusSearch() }

    // What the hero shows with nothing picked: the last played instance, else the first.
    readonly property string defaultHeroId: root.recentModel && root.recentModel.firstId ? root.recentModel.firstId
        : root.instanceModel && root.instanceModel.firstId ? root.instanceModel.firstId : ""

    readonly property var newsEntries: side.newsEntries
    readonly property var featuredNews: side.featuredNews
    readonly property bool hasNews: side.hasNews

    property string viewMode: "grid"
    readonly property string searchText: toolbar.searchText
    readonly property bool searching: root.searchText.length > 0

    // Main.qml keeps the play bar away until the hero's Play has scrolled away.
    readonly property bool heroPlayInView: hero.visible && flick.visible
                                           && flick.contentY < hero.y + hero.playCenterY

    readonly property string look: Theme.look
    readonly property bool isStage: root.look === "stage"
    readonly property bool isNight: root.look === "night"
    readonly property int stagePadding: 44
    readonly property int openPadding: 40
    readonly property int pagePadding: root.isStage ? root.stagePadding
                                     : root.look === "classic" ? Theme.space.xl + Theme.space.xs
                                     : root.openPadding
    readonly property int emptyStateWidth: 400
    readonly property int libraryCount: root.instanceModel && root.instanceModel.count !== undefined ? root.instanceModel.count : 0
    readonly property bool hasLibrary: root.libraryCount > 0 || root.searching

    // Shown only while the main column keeps sideMinMainWidth beside it. Gece's glass pane is inset,
    // so the scroll bar runs in the gap before it.
    readonly property int nightSidePaneWidth: 282
    readonly property int classicSidePaneWidth: 300
    readonly property int sidePaneWidth: root.isNight ? root.nightSidePaneWidth : root.classicSidePaneWidth
    readonly property int sideInset: root.isNight ? Theme.space.xl + Theme.space.xs + Theme.space.xxs : 0
    readonly property int sideWidth: root.sidePaneWidth + root.sideInset * 2
    readonly property int sideMinMainWidth: 700
    readonly property bool showSide: !root.isStage && side.hasContent
                                     && root.width - root.sideWidth - root.pagePadding * 2 >= root.sideMinMainWidth

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

        MouseArea {
            width: flick.width
            height: Math.max(flick.height, flick.contentHeight)
            onClicked: root.selectRequested("")
        }

    // Spans the page so a look can bleed to its edges; pads its own top.
        HomeHero {
            id: hero
            x: 0
            y: 0
            width: flick.width
            columnX: root.contentX
            columnWidth: root.contentWidth
            rowModel: root.heroModel ? root.heroModel : root.recentModel
            launchOptionsFor: root.launchOptionsFor
            worldsModel: root.recentWorldsModel
            selectedId: root.selectedId
            lastPlayedId: root.defaultHeroId
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

        Column {
            id: content
            x: root.contentX
            y: hero.visible ? hero.y + hero.height + hero.gapBelow : Theme.space.xl
            width: root.contentWidth
            spacing: Theme.space.xxl

            HomeContinuePlaying {
                width: parent.width
                recentModel: root.recentModel
                instanceModel: root.instanceModel
                // Not the hero's own: picking a row must not move the rows under the pointer.
                excludeId: root.defaultHeroId
                selectedId: root.selectedId
                frozen: root.searching
                onSelectRequested: (id) => root.selectRequested(id)
                onOpenRequested: (id) => root.openInstanceRequested(id)
                onLaunchRequested: (id) => root.launchRequested(id)
                onStopRequested: (id) => root.stopRequested(id)
                onMenuRequested: (id, running, name, iconKey, anchor) => {
                    sections.openInstanceMenu(id, running, anchor, name, iconKey)
                    root.menuRequested(id, anchor)
                }
            }

            HomeStageSections {
                id: stageSections
                width: parent.width
                visible: root.isStage && stageSections.hasContent
                hasInstances: root.hasLibrary
                signedIn: root.msaSignedIn
                importer: root.launcherImport
                news: root.news
                onSignInRequested: root.signInRequested()
                onImportRequested: root.importLaunchersRequested()
            }

            Column {
                width: parent.width
                spacing: Theme.space.lg

                Column {
                    width: parent.width
                    spacing: Theme.space.md

                    HomeSectionTitle {
                        width: parent.width
                        text: qsTr("Library")
                        count: root.libraryCount
                        detail: root.libraryCount === 1 ? qsTr("1 instance")
                              : root.libraryCount > 1 ? qsTr("%1 instances").arg(root.libraryCount) : ""
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
                    shareCodes: root.shareCodes
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
                            size: Theme.icon.xl
                            color: Theme.palette.textTertiary
                        }
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: undefined
        anchors.bottom: parent.bottom
        width: root.mainWidth
        visible: root.hasLibrary
        flickable: flick
    }

    HomeEmptyState {
        x: Math.round((root.mainWidth - width) / 2)
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(root.mainWidth - root.pagePadding * 2, root.emptyStateWidth)
        visible: !root.hasLibrary
        onCreateRequested: root.createRequested()
        onDiscoverRequested: root.discoverRequested()
    }

    HomeSidePanel {
        id: side
        // Gece's pane is as tall as its content and inset from the page's edges; the others run the full height.
        x: root.width - width - root.sideInset
        y: root.sideInset
        width: root.sidePaneWidth
        height: root.isNight ? Math.min(side.naturalHeight, root.height - root.sideInset * 2)
                             : root.height
        visible: root.showSide
        hasInstances: root.hasLibrary
        signedIn: root.msaSignedIn
        importer: root.launcherImport
        news: root.news
        recentWorldsModel: root.recentWorldsModel
        friends: root.friends
        friendPlaces: root.friendPlaces
        joinFlow: root.joinFlow
        friendsInPanel: root.friendsInPanel
        onFriendsRequested: (tab) => root.friendsRequested(tab)
        onNavigateRequested: (target) => root.navigateRequested(target)
        onJoinRequested: (friend) => root.joinRequested(friend)
        onSignInRequested: root.signInRequested()
        onImportRequested: root.importLaunchersRequested()
        onWorldRequested: (id) => root.openInstanceWorldsRequested(id)
    }
}
