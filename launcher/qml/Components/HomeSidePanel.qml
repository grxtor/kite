// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Home's side panel: "Set up Kite" (GettingStartedCard) until both its
 * steps are done, then Friends (HomeFriends, unless the friends panel on the
 * window's right edge is opened out and has them), then Kite's news, then the
 * worlds played most recently across every instance (compact HomeWorldTile
 * rows). Its own column
 * beside the page, scrolling on its own and set off by a hairline; the page
 * decides whether there is room for it, and hides it when it has nothing to
 * show (`hasContent`).
 */
Item {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // QmlShell.news (NewsModel), or null.
    property var news: null
    // QmlShell.recentWorlds (RecentWorldsModel), or null.
    property var recentWorldsModel: null
    // QmlShell.friends (FriendsController), or null.
    property var friends: null
    // Main.qml's FriendJoinFlow.
    property var joinFlow: null
    // The friends panel on the window's right edge is opened out, with the
    // same friends in it: this panel leaves them to it.
    property bool friendsInPanel: false

    signal createRequested()
    signal signInRequested()
    // The Friends page, on "friends" or "requests", or "add" (the add
    // field focused).
    signal friendsRequested(string tab)
    // A path for Main.qml's navigate() ("settings/friends").
    signal navigateRequested(string target)
    // Join a friend's server (a friend map).
    signal joinRequested(var friend)
    // A recent world: open its instance on the Worlds tab.
    signal worldRequested(string instanceId)

    readonly property int maxWorlds: 4
    readonly property bool hasWorlds: worldsRepeater.count > 0

    // Whether the friends show here: not while the right-edge panel, opened
    // out, has them.
    readonly property bool showsFriends: friendsSection.shown && root.hasInstances && !root.friendsInPanel

    // Not the children's `visible`: that follows the panel's own.
    readonly property bool hasContent: gettingStarted.shown || newsList.hasNews || root.hasWorlds
                                     || root.showsFriends
    // The news as HomeNews reads it: every post off `news`, the newest one
    // still worth showing (or null), and whether there is one.
    readonly property var newsEntries: newsList.allEntries
    readonly property var featuredNews: newsList.entries.length > 0 ? newsList.entries[0] : null
    readonly property bool hasNews: newsList.hasNews

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.divider
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.leftMargin: 1
        contentWidth: width
        contentHeight: column.y + column.height + Theme.space.xl
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: Theme.space.lg
            y: Theme.space.xl
            width: flick.width - Theme.space.lg * 2
            spacing: Theme.space.xl

            GettingStartedCard {
                id: gettingStarted
                width: parent.width
                visible: gettingStarted.shown
                hasInstances: root.hasInstances
                signedIn: root.signedIn
                onCreateRequested: root.createRequested()
                onSignInRequested: root.signInRequested()
            }

            HomeFriends {
                id: friendsSection
                width: parent.width
                // Like the checklist, after the first instance: the empty
                // library has the page's own way in in front of it.
                visible: root.showsFriends
                controller: root.friends
                joinFlow: root.joinFlow
                setupShowsSignIn: gettingStarted.shown && !root.signedIn
                onManageRequested: (tab) => root.friendsRequested(tab)
                onSignInRequested: root.signInRequested()
                onNavigateRequested: (target) => root.navigateRequested(target)
                onJoinRequested: (friend) => root.joinRequested(friend)
            }

            HomeNews {
                id: newsList
                width: parent.width
                news: root.news
            }

            Column {
                visible: root.hasWorlds
                width: parent.width
                spacing: Theme.space.xs

                HomePanelHeading {
                    width: parent.width
                    bottomPadding: Theme.space.xs
                    text: qsTr("Recent worlds")
                }

                Repeater {
                    id: worldsRepeater
                    model: root.recentWorldsModel
                    // A plain wrapper, so `model.index` resolves despite
                    // HomeWorldTile's required properties.
                    delegate: Item {
                        visible: model.index < root.maxWorlds
                        width: visible ? column.width : 0
                        height: visible ? tile.height : 0

                        // The icon stands on the column's edge, with the
                        // heading and the cards; the row's hover bleeds out
                        // by its own padding.
                        HomeWorldTile {
                            id: tile
                            x: -Theme.space.sm
                            width: column.width + Theme.space.sm * 2
                            worldName: model.worldName
                            folderName: model.folderName
                            iconUrl: model.iconUrl
                            lastPlayed: model.lastPlayed
                            instanceId: model.instanceId
                            instanceName: model.instanceName
                            onClicked: root.worldRequested(model.instanceId)
                        }
                    }
                }
            }
        }
    }
}
