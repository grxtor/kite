// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Your friends, fixed on the window's right edge on every page, as Lunar's
 * launcher has it. It starts as a slim strip of heads (FriendsPanelStrip)
 * that takes next to no room; whoever wants names, groups and Join in plain
 * sight opens it out (FriendsPanelFull) and drags its left edge to the width
 * they like (FriendsPanelHandle). Releasing the drag near the strip's width
 * folds it back, a double click on the edge or the buttons in either mode
 * switch, and the choice and the width are remembered (Main.qml keeps both
 * as settings; this only asks, with `expandedRequested` and `widthCommitted`).
 *
 * Removing or blocking someone is the ⋯ on a row, or on a head's card in the
 * strip, with the same question the Friends page asks. What does not fit a
 * narrow column stays on the page (Main.qml's "friends"): requests, blocked
 * people and adding someone by name; the buttons for those go there.
 *
 * Main.qml sets `open` from its top bar's toggle and the FriendsPanelOpen
 * setting; closing slides the width to nothing, and the content keeps its own
 * width while it does, so nothing reflows on the way. Folding and unfolding
 * slide the same way, with the heads and the names fading across; dragging
 * follows the pointer without any animation. Both bodies are built once and
 * never torn down, so the folds in the full panel hold for the session.
 *
 * Home's own side panel leaves the friends to this one only while it is
 * opened out (`expanded`): a strip of heads is not a list to read.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Main.qml's FriendJoinFlow, which says what a friend's Join starts.
    property var joinFlow: null
    // Whether the panel is out.
    property bool open: false
    // Opened out (names and groups) rather than a strip of heads. Main.qml
    // keeps the choice; this asks for a change with expandedRequested.
    property bool expanded: false
    // The width the user gave the opened-out panel, as saved; it is brought
    // into range (see fullWidth) rather than trusted.
    property int expandedWidth: 320
    // The width the page area and this panel share: the window's, less what
    // stands to the left of them. The panel never leaves the page narrower
    // than minPageWidth.
    property int roomWidth: 0

    // The Friends page, on "friends", "requests" or "blocked", or "add" (the
    // add field focused).
    signal friendsRequested(string tab)
    // A path for Main.qml's navigate().
    signal navigateRequested(string target)
    // Join this friend's server (a friend map); Main.qml picks the instance.
    signal joinRequested(var friend)
    // The strip or the opened-out panel was asked for (a button, a double
    // click on the edge, or a drag that ended near or far from the strip).
    signal expandedRequested(bool expanded)
    // A drag ended with the panel opened out, at this width.
    signal widthCommitted(int width)

    // A strip: a head's own square, with the room around it that its hover
    // fill and the presence dot need.
    readonly property int stripWidth: Theme.control.heightLg + Theme.space.sm * 2
    // Narrower than this and a name and Join no longer share a row.
    readonly property int minExpandedWidth: 260
    readonly property int maxExpandedWidth: 440
    readonly property int minPageWidth: 480
    // Releasing a drag below this folds the panel to the strip: half way
    // between the strip and the narrowest opened-out panel.
    readonly property int snapWidth: root.stripWidth + Math.round((root.minExpandedWidth - root.stripWidth) / 2)

    // The widest the panel may be: not past maxExpandedWidth, nor leaving
    // the page narrower than minPageWidth -- except that the opened-out
    // panel is never narrower than it can be used.
    readonly property int maxWidth: Math.max(root.minExpandedWidth,
                                             Math.min(root.maxExpandedWidth,
                                                      root.roomWidth > 0 ? root.roomWidth - root.minPageWidth
                                                                         : root.maxExpandedWidth))
    // The saved width, brought into range: the window may have shrunk since.
    readonly property int fullWidth: Math.min(Math.max(root.expandedWidth, root.minExpandedWidth), root.maxWidth)

    // A drag in progress, and where it has taken the edge.
    property bool dragging: false
    property int dragWidth: 0
    // What is on show: the opened-out panel, or -- while dragging -- the
    // one that letting go now would leave.
    readonly property bool showsFull: root.dragging ? root.dragWidth >= root.snapWidth : root.expanded

    // "off", "noAccount", "signingIn", "error" or "ready" (Format.friendsPhase()).
    readonly property string phase: Format.friendsPhase(root.controller)
    readonly property bool connected: root.phase === "ready"
    // The lists are shown, if not trusted: connected, or failing after they
    // were read.
    readonly property bool stale: Format.friendsStale(root.controller)
    readonly property bool showsLists: root.connected || root.stale
    // The list has been read: before that it is empty for want of an answer,
    // which says nothing about whether there are friends.
    readonly property bool loaded: !!root.controller && root.controller.loaded
    readonly property int friendCount: root.controller ? root.controller.friends.length : 0
    readonly property var groups: Format.friendsGrouped(root.controller ? root.controller.friends : [])

    // Fresh lists whenever the panel is shown. Only while connected: asking
    // while friends are off or failing only makes the controller answer with
    // a toast saying what the panel already says. (A panel that opens before
    // friends connect has nothing to ask yet; the controller reads the lists
    // itself as it connects.)
    function load() {
        if (!root.controller || !root.connected)
            return
        root.controller.refresh()
    }
    onOpenChanged: if (root.open) root.load()
    Component.onCompleted: {
        if (root.open)
            root.load()
        // The width is set at start-up from the saved settings, and should
        // appear in place; only a change after that slides.
        Qt.callLater(() => root.settled = true)
    }

    // The pointer wants the left edge at @p edge (in this panel's parent, as
    // FriendsPanelHandle measures it): the panel is as wide as what is left
    // of that up to its right edge, within what it may be.
    function dragTo(edge) {
        var wanted = Math.round(root.x + root.width - edge)
        root.dragWidth = Math.min(Math.max(wanted, root.stripWidth), root.maxWidth)
    }
    function endDrag() {
        // Say what was chosen before the drag stops steering the width, so
        // the width settles on the new mode in one move.
        if (root.dragWidth < root.snapWidth) {
            if (root.expanded)
                root.expandedRequested(false)
        } else {
            root.widthCommitted(Math.max(root.dragWidth, root.minExpandedWidth))
            if (!root.expanded)
                root.expandedRequested(true)
        }
        // The slide is switched back on first: letting go makes the width
        // change in the same breath, and a binding that has not yet heard that
        // the drag is over would write it with the slide still off, jumping.
        root.pointerSteers = false
        root.dragging = false
    }

    // True from the first turn of the event loop.
    property bool settled: false
    // The width follows the pointer, so it must not slide after it. True for
    // as long as `dragging` is, but cleared a moment before it (see endDrag),
    // which is why it is a property of its own.
    property bool pointerSteers: false

    width: !root.open ? 0 : root.dragging ? root.dragWidth : root.expanded ? root.fullWidth : root.stripWidth
    visible: root.width > 0
    clip: true
    Behavior on width {
        enabled: root.settled && !root.pointerSteers
        NumberAnimation {
            duration: SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.normal
            easing.type: Theme.motion.easing
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.palette.canvas
    }
    // Set off from the page by a hairline, as Home's side panel is, and
    // the page panel's top edge carries on across.
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

    // Each body keeps its own width at the right edge while the panel's
    // slides, and the other fades in over it: nothing reflows on the way.
    FriendsPanelStrip {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.stripWidth
        opacity: root.showsFull ? 0 : 1
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.fast
                easing.type: Theme.motion.easing
            }
        }

        controller: root.controller
        joinFlow: root.joinFlow
        menu: personMenu
        phase: root.phase
        showsLists: root.showsLists
        stale: root.stale
        connected: root.connected
        groups: root.groups
        incomingCount: root.controller ? root.controller.incomingCount : 0
        onExpandRequested: root.expandedRequested(true)
        onAddRequested: root.friendsRequested("add")
        onRequestsRequested: root.friendsRequested("requests")
        onJoinRequested: (friend) => root.joinRequested(friend)
    }

    FriendsPanelFull {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        // Follows the pointer while dragging, so the names reflow with it;
        // otherwise the width the panel has when it is opened out, however
        // far its own is on the way.
        width: root.dragging ? Math.max(root.dragWidth, root.minExpandedWidth) : root.fullWidth
        opacity: root.showsFull ? 1 : 0
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation {
                duration: SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.fast
                easing.type: Theme.motion.easing
            }
        }

        controller: root.controller
        joinFlow: root.joinFlow
        menu: personMenu
        phase: root.phase
        connected: root.connected
        stale: root.stale
        showsLists: root.showsLists
        loaded: root.loaded
        friendCount: root.friendCount
        groups: root.groups
        onFriendsRequested: (tab) => root.friendsRequested(tab)
        onNavigateRequested: (target) => root.navigateRequested(target)
        onJoinRequested: (friend) => root.joinRequested(friend)
        onCollapseRequested: root.expandedRequested(false)
    }

    FriendsPanelHandle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        space: root.parent
        onDragStarted: {
            root.dragWidth = root.width
            root.pointerSteers = true
            root.dragging = true
        }
        onDragged: (edge) => root.dragTo(edge)
        onDragFinished: root.endDrag()
        onToggleRequested: root.expandedRequested(!root.expanded)
    }

    // What a row's ⋯ (or a card's) opens. One for the panel, not one per
    // row: the lists are rebuilt whenever they are read again, and a menu in
    // a row would close under the pointer.
    FriendPersonMenu {
        id: personMenu
        onRemoveRequested: (uuid, name) => confirm.ask("remove", uuid, name)
        onBlockRequested: (uuid, name, friend) => confirm.ask(friend ? "block" : "blockRequest", uuid, name)
    }

    // Removing a friend and blocking someone, both after a question.
    FriendConfirmDialog {
        id: confirm
        controller: root.controller
    }
}
