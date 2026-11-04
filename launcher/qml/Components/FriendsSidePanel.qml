// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Your friends, fixed on the window's right edge: a strip of heads (FriendsPanelStrip) or opened
 * out to a list (FriendsPanelFull), with a draggable left edge (PanelEdgeHandle). Main.qml keeps
 * the mode and width as settings; this only asks, via expandedRequested and widthCommitted.
 * openChat() opens a conversation in it, opening it out first; the conversation counts as read
 * only while it is on screen in the active window.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    // Where friends play (Main.qml's FriendPlaces): server names and favicons.
    property FriendPlaces places: FriendPlaces {}
    property var joinFlow: null
    property bool open: false
    // Main.qml keeps the choice; this asks via expandedRequested.
    property bool expanded: false
    // The saved width; brought into range, see fullWidth.
    property int expandedWidth: 320
    // The width the page area and this panel share; the panel never leaves the page under minPageWidth.
    property int roomWidth: 0
    // The launcher account's head, for its own messages.
    property string myFace

    // tab: "friends", "requests", "blocked" or "add".
    signal friendsRequested(string tab)
    signal navigateRequested(string target)
    signal joinRequested(var friend)
    signal expandedRequested(bool expanded)
    signal widthCommitted(int width)

    readonly property int stripWidth: Theme.control.heightLg + Theme.space.sm * 2
    // Narrower than this and a name and Join no longer share a row.
    readonly property int minExpandedWidth: 260
    readonly property int maxExpandedWidth: 440
    readonly property int minPageWidth: 480
    readonly property int snapWidth: root.stripWidth + Math.round((root.minExpandedWidth - root.stripWidth) / 2)

    // Not past maxExpandedWidth nor leaving the page under minPageWidth, but never below minExpandedWidth.
    readonly property int maxWidth: Math.max(root.minExpandedWidth,
                                             Math.min(root.maxExpandedWidth,
                                                      root.roomWidth > 0 ? root.roomWidth - root.minPageWidth
                                                                         : root.maxExpandedWidth))
    // The window may have shrunk since the width was saved.
    readonly property int fullWidth: Math.min(Math.max(root.expandedWidth, root.minExpandedWidth), root.maxWidth)

    property bool dragging: false
    property int dragWidth: 0
    readonly property bool showsFull: root.dragging ? root.dragWidth >= root.snapWidth : root.expanded
    // From the snap point on at least minExpandedWidth: the names are laid out at that
    // width and would be cut off in a narrower panel.
    readonly property int dragLiveWidth: root.dragWidth < root.snapWidth ? root.dragWidth
                                                                         : Math.max(root.dragWidth, root.minExpandedWidth)

    readonly property FriendsState friendsState: FriendsState { controller: root.controller }

    // Asks only while connected: otherwise the controller answers with a toast the panel already says.
    function load() {
        if (!root.controller || !root.friendsState.connected)
            return
        root.controller.refresh()
    }
    onOpenChanged: if (root.open) root.load()

    readonly property var chat: root.controller ? root.controller.chat : null
    // The conversation with @p uuid, in the opened-out panel.
    function openChat(uuid) {
        if (!root.chat)
            return
        root.chat.open(uuid)
        if (!root.expanded)
            root.expandedRequested(true)
    }
    Binding {
        when: !!root.chat
        target: root.chat
        property: "viewing"
        value: root.open && root.showsFull && Qt.application.state === Qt.ApplicationActive
        restoreMode: Binding.RestoreNone
    }
    Component.onCompleted: {
        if (root.open)
            root.load()
        // Set at start-up from the saved settings, so it appears in place; only later changes slide.
        Qt.callLater(() => root.settled = true)
    }

    // @p edge: the pointer's x in this panel's parent, as PanelEdgeHandle measures it.
    function dragTo(edge) {
        var wanted = Math.round(root.x + root.width - edge)
        root.dragWidth = Math.min(Math.max(wanted, root.stripWidth), root.maxWidth)
    }
    function endDrag() {
        // Say the choice before the drag ends so the panel settles on the new mode in one move.
        if (root.dragWidth < root.snapWidth) {
            if (root.expanded)
                root.expandedRequested(false)
        } else {
            root.widthCommitted(Math.max(root.dragWidth, root.minExpandedWidth))
            if (!root.expanded)
                root.expandedRequested(true)
        }
        root.dragging = false
    }

    property bool settled: false

    // The slide belongs to a change of this, not of the width: when the window or rail resizes the
    // panel must follow at once, or the page would sit under minPageWidth until the slide caught up.
    readonly property string mode: !root.open ? "closed"
                                   : root.dragging ? "drag"
                                   : root.expanded ? "full" : "strip"

    visible: root.width > 0
    clip: true
    // A PropertyChanges binding follows its expression while its state holds, so only a move between states slides.
    states: [
        State {
            name: "closed"
            when: root.mode === "closed"
            PropertyChanges { root.width: 0 }
        },
        State {
            name: "strip"
            when: root.mode === "strip"
            PropertyChanges { root.width: root.stripWidth }
        },
        State {
            name: "full"
            when: root.mode === "full"
            PropertyChanges { root.width: root.fullWidth }
        },
        State {
            name: "drag"
            when: root.mode === "drag"
            PropertyChanges { root.width: root.dragLiveWidth }
        }
    ]
    transitions: Transition {
        to: "closed,strip,full"
        enabled: root.settled
        NumberAnimation {
            property: "width"
            duration: Theme.motion.normal
            easing.type: Theme.motion.easing
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.look === "night" ? "transparent" : Theme.palette.canvas
    }
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.divider
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.palette.border
    }

    // Each body keeps its own width at the right edge while the panel's slides, and the other fades in over it.
    FriendsPanelStrip {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.stripWidth
        opacity: root.showsFull ? 0 : 1
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        friendsState: root.friendsState
        places: root.places
        joinFlow: root.joinFlow
        menu: personMenu
        onExpandRequested: root.expandedRequested(true)
        onAddRequested: root.friendsRequested("add")
        onRequestsRequested: root.friendsRequested("requests")
        onJoinRequested: (friend) => root.joinRequested(friend)
        onMessageRequested: (uuid) => root.openChat(uuid)
    }

    FriendsPanelFull {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.dragging ? Math.max(root.dragWidth, root.minExpandedWidth) : root.fullWidth
        opacity: root.showsFull ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        friendsState: root.friendsState
        places: root.places
        joinFlow: root.joinFlow
        menu: personMenu
        myFace: root.myFace
        onFriendsRequested: (tab) => root.friendsRequested(tab)
        onNavigateRequested: (target) => root.navigateRequested(target)
        onJoinRequested: (friend) => root.joinRequested(friend)
        onCollapseRequested: root.expandedRequested(false)
    }

    PanelEdgeHandle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        space: root.parent
        onDragStarted: {
            root.dragWidth = root.width
            root.dragging = true
        }
        onDragged: (edge) => root.dragTo(edge)
        onDragFinished: root.endDrag()
        onToggleRequested: root.expandedRequested(!root.expanded)
    }

    // One menu for the panel, not one per row: the lists are rebuilt on every read and a menu in a row would close
    // under the pointer.
    FriendPersonMenu {
        id: personMenu
        onMessageRequested: (uuid) => root.openChat(uuid)
        onRemoveRequested: (uuid, name) => confirm.ask("remove", uuid, name)
        onBlockRequested: (uuid, name, friend) => confirm.ask(friend ? "block" : "blockRequest", uuid, name)
    }

    FriendConfirmDialog {
        id: confirm
        controller: root.controller
    }
}
