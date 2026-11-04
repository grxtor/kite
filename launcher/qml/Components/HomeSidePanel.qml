// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Home's side panel: "Set up Kite" until every step is done, then Friends (unless the right-edge friends panel
 * has them), Kite's news and recent worlds. The page decides whether there is room and hides it when it has
 * nothing (`hasContent`). Gece makes it one glass pane as tall as its content (`naturalHeight`).
 */
Item {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // NewsModel, or null.
    property var news: null
    // RecentWorldsModel, or null.
    property var recentWorldsModel: null
    // FriendsController, or null.
    property var friends: null
    // Main.qml's FriendPlaces.
    property FriendPlaces friendPlaces: FriendPlaces {}
    property var joinFlow: null
    // The right-edge friends panel is out with the same friends: this panel leaves them to it.
    property bool friendsInPanel: false

    signal signInRequested()
    // tab: "friends", "requests" or "add".
    signal friendsRequested(string tab)
    signal navigateRequested(string target)
    signal joinRequested(var friend)
    signal worldRequested(string instanceId)

    readonly property bool glass: Theme.look === "night"
    readonly property int padding: Theme.look === "calm" ? Theme.space.xl + Theme.space.xs
                                 : root.glass ? Theme.space.lg + Theme.space.xs : Theme.space.lg
    readonly property int topPadding: Theme.look === "calm" ? Theme.space.xxl + Theme.space.xs - 2
                                    : root.glass ? Theme.space.lg + Theme.space.xs : Theme.space.xl
    readonly property real naturalHeight: column.y + column.height + root.topPadding
    readonly property bool hasWorlds: worldList.hasWorlds

    readonly property bool showsFriends: friendsSection.shown && root.hasInstances && !root.friendsInPanel

    // Not the children's `visible`: that follows the panel's own.
    readonly property bool hasContent: gettingStarted.shown || newsList.hasNews || root.hasWorlds
                                     || root.showsFriends
    readonly property var newsEntries: newsList.allEntries
    readonly property var featuredNews: newsList.entries.length > 0 ? newsList.entries[0] : null
    readonly property bool hasNews: newsList.hasNews

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.divider
        visible: !root.glass
    }
    HomeGlass {
        anchors.fill: parent
        visible: root.glass
        radius: Theme.radius.xl
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.leftMargin: root.glass ? 0 : 1
        contentWidth: width
        contentHeight: column.y + column.height + root.topPadding
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: root.padding
            y: root.topPadding
            width: flick.width - root.padding * 2
            spacing: Theme.space.xl

            GettingStartedCard {
                id: gettingStarted
                flat: Theme.look === "calm" || root.glass
                width: parent.width
                visible: gettingStarted.shown
                hasInstances: root.hasInstances
                signedIn: root.signedIn
                onSignInRequested: root.signInRequested()
            }

            HomeFriends {
                id: friendsSection
                width: parent.width
                visible: root.showsFriends
                controller: root.friends
                places: root.friendPlaces
                joinFlow: root.joinFlow
                setupShowsSignIn: gettingStarted.shown && !root.signedIn
                onManageRequested: (tab) => root.friendsRequested(tab)
                onSignInRequested: root.signInRequested()
                onNavigateRequested: (target) => root.navigateRequested(target)
                onJoinRequested: (friend) => root.joinRequested(friend)
            }

            HomeNews {
                id: newsList
                flat: Theme.look === "calm" || root.glass
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

                HomeWorldList {
                    id: worldList
                    width: parent.width
                    spacing: Theme.space.xs
                    worldsModel: root.recentWorldsModel
                    glass: root.glass
                    inset: Theme.space.sm
                    onWorldRequested: (id) => root.worldRequested(id)
                }
            }
        }
    }
}
