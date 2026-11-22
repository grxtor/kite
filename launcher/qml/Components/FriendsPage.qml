// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The friends list in full: you (FriendsSelfCard), the Friends, Requests and Blocked tabs, a toolbar (search the
 * tab, add someone by name) and the tab's people in cards. While friends are not connected the list gives way to
 * FriendsPageState; a poll that failed after the lists were read keeps them under a bar, and nothing that needs the
 * server works until it answers.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    // Where friends play (Main.qml's FriendPlaces): server names and favicons.
    property FriendPlaces places: FriendPlaces {}
    // Your own head ("image://accountface/..."); the launcher already has it.
    property string myFace: ""
    property var joinFlow: null
    property string tab: "friends"
    // Main.qml binds this to its PageSlot's StackLayout.isCurrentItem.
    property bool shown: false

    signal navigateRequested(string target)
    signal joinRequested(var friend)
    // Message in a friend's ⋯: the conversation opens in the friends panel.
    signal messageRequested(string uuid)

    readonly property FriendsState friendsState: FriendsState { controller: root.controller }
    readonly property string updatedAt: Format.friendsUpdatedAt(root.controller)
    // With no friends, requests or blocks there is nobody to search for.
    readonly property bool hasAnyone: Format.friendsHasAnyone(root.controller, true)
    // What the search field holds; the tabs show the people whose name has it.
    property string query
    // Under this the search and the add field take a line each.
    readonly property int toolbarRowWidth: 560
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    // Also fills the Blocked tab's count before it is opened. Only while connected: otherwise the
    // controller answers with a toast the page already says.
    function load() {
        if (!root.controller || !root.friendsState.connected)
            return
        root.controller.refresh()
        root.controller.refreshBlocked()
    }
    function focusAddField() {
        addField.focusField()
    }
    // @p mode: "remove", "block", or "blockRequest" (the person is not a friend: their request goes).
    function confirmAction(mode, uuid, name) {
        confirm.ask(mode, uuid, name)
    }

    onShownChanged: if (root.shown) root.load()
    onTabChanged: root.query = ""
    onQueryChanged: if (search.text !== root.query) search.text = root.query
    Component.onCompleted: if (root.shown) root.load()

    Connections {
        target: root.friendsState
        function onConnectedChanged() {
            if (root.shown)
                root.load()
        }
    }

    ColumnLayout {
        id: content
        visible: root.friendsState.showsLists
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        anchors.topMargin: Theme.space.xl
        spacing: Theme.space.lg

        FriendsStaleBar {
            Layout.fillWidth: true
            visible: root.friendsState.stale
            controller: root.controller
            updatedAt: root.updatedAt
        }

        FriendsNotice {
            Layout.fillWidth: true
            controller: root.controller
            onSettingsRequested: root.navigateRequested("settings/friends")
        }

        FriendsSelfCard {
            Layout.fillWidth: true
            controller: root.controller
            places: root.places
            face: root.myFace
            online: root.friendsState.connected
            onSettingsRequested: root.navigateRequested("settings/friends")
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.md

            TabStrip {
                Layout.fillWidth: true
                current: root.tab
                onActivated: (id) => root.tab = id
                tabs: [
                    { id: "friends", label: qsTr("Friends"), count: root.controller ? root.controller.friends.length : 0 },
                    // Every row of the tab is counted, so the number is never short of the list; the pill is for
                    // the ones that wait on you.
                    { id: "requests", label: qsTr("Requests"),
                      count: root.controller ? root.controller.incoming.length + root.controller.outgoing.length : 0,
                      attention: !!root.controller && root.controller.incomingCount > 0 },
                    { id: "blocked", label: qsTr("Blocked"), count: root.controller ? root.controller.blocked.length : 0 }
                ]
            }

            GridLayout {
                Layout.fillWidth: true
                columns: root.width - root.sideMargin * 2 >= root.toolbarRowWidth ? 3 : 1
                columnSpacing: Theme.space.sm
                rowSpacing: Theme.space.sm

                SearchBox {
                    id: search
                    objectName: "friendSearch"
                    Layout.fillWidth: true
                    Layout.maximumWidth: parent.columns > 1 ? 280 : Number.POSITIVE_INFINITY
                    Layout.alignment: Qt.AlignTop
                    visible: root.hasAnyone
                    placeholderText: root.tab === "requests" ? qsTr("Search requests")
                                   : root.tab === "blocked" ? qsTr("Search blocked people") : qsTr("Search friends")
                    onTextChanged: root.query = search.text
                }
                Item {
                    Layout.fillWidth: true
                    visible: parent.columns > 1
                }
                FriendsAddField {
                    id: addField
                    Layout.fillWidth: parent.columns === 1
                    Layout.alignment: Qt.AlignTop
                    controller: root.controller
                    online: root.friendsState.connected
                }
            }

            StackLayout {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.tab === "requests" ? 1 : root.tab === "blocked" ? 2 : 0
                // Not a hard swap: the tab's list comes in.
                onCurrentIndexChanged: tabStack.children[tabStack.currentIndex].enter()

                FriendsFriendsTab {
                    controller: root.controller
                    friendsState: root.friendsState
                    places: root.places
                    menu: personMenu
                    joinFlow: root.joinFlow
                    stale: root.friendsState.stale
                    actionsEnabled: root.friendsState.connected
                    query: root.query
                    onJoinRequested: (friend) => root.joinRequested(friend)
                    onMessageRequested: (uuid) => root.messageRequested(uuid)
                    onAddRequested: root.focusAddField()
                    onClearRequested: root.query = ""
                }
                FriendsRequestsTab {
                    controller: root.controller
                    friendsState: root.friendsState
                    menu: personMenu
                    actionsEnabled: root.friendsState.connected
                    query: root.query
                    onClearRequested: root.query = ""
                }
                FriendsBlockedTab {
                    controller: root.controller
                    friendsState: root.friendsState
                    actionsEnabled: root.friendsState.connected
                    query: root.query
                    onClearRequested: root.query = ""
                }
            }
        }
    }

    FriendsPageState {
        anchors.fill: parent
        visible: !root.friendsState.showsLists
        friendsState: root.friendsState
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // One menu for the page, not one per row: the lists are rebuilt on every read and a menu in a row would close
    // under the pointer.
    FriendPersonMenu {
        id: personMenu
        onMessageRequested: (uuid) => root.messageRequested(uuid)
        controller: root.controller
        places: root.places
        onRemoveRequested: (uuid, name) => root.confirmAction("remove", uuid, name)
        onBlockRequested: (uuid, name, friend) => root.confirmAction(friend ? "block" : "blockRequest", uuid, name)
    }

    FriendConfirmDialog {
        id: confirm
        controller: root.controller
    }
}
