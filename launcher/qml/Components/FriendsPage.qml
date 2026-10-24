// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The friends list in full, its own page in the rail: add someone by their
 * Minecraft name, then three tabs -- Friends (where each one is; ⋯ to remove
 * or block), Requests (accept, decline, cancel) and Blocked (unblock).
 * Removing and blocking ask first. What the server answered comes back as a
 * toast from Main.qml, off FriendsController.actionFinished. While friends
 * are not connected the list gives way to FriendsPageState.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // "friends", "requests" or "blocked".
    property string tab: "friends"
    // Whether this is the page on screen. Main.qml binds it to its PageSlot's
    // StackLayout.isCurrentItem (see DiscoverPage.shown).
    property bool shown: false

    // A path for Main.qml's navigate().
    signal navigateRequested(string target)

    // "off", "noAccount", "signingIn", "error" or "ready"; as HomeFriends
    // reads it, the controller's own "off" while it is enabled is only the
    // moment before it connects.
    readonly property string phase: !root.controller ? "off"
        : !root.controller.enabled ? "off"
        : root.controller.state === "off" ? "signingIn" : root.controller.state
    readonly property bool connected: root.phase === "ready"
    readonly property string typedName: nameField.text.trim()
    // Minecraft's own rule for a name.
    readonly property bool nameValid: /^[A-Za-z0-9_]{1,16}$/.test(root.typedName)
    // Past this the add row and the list stop reading as one block; the
    // column stops growing here on wide windows, as SettingsScroll's does.
    readonly property int readableWidth: 760
    // A request is on its way; the name stays in the field until it is
    // accepted, so a failed one is not typed again.
    property bool sending: false

    function openTab(id) {
        root.tab = id
    }
    // Fresh lists whenever the page is shown, and the Blocked tab's count
    // before it is ever opened. Only while connected: asking while friends
    // are off or failing only makes the controller answer with a toast
    // saying what the page already says.
    function load() {
        if (!root.controller || !root.connected)
            return
        root.controller.refresh()
        root.controller.refreshBlocked()
    }
    function submit() {
        if (!root.nameValid || !root.controller)
            return
        root.sending = true
        root.controller.sendRequest(root.typedName)
    }
    function confirmAction(remove, uuid, name) {
        confirm.removing = remove
        confirm.targetUuid = uuid
        confirm.targetName = name
        confirm.open()
    }

    onShownChanged: if (root.shown) root.load()
    // Connecting while the page is open (after Turn on, or Try again).
    onConnectedChanged: if (root.shown) root.load()
    Component.onCompleted: if (root.shown) root.load()

    ColumnLayout {
        id: content
        visible: root.connected
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        // The page area gives no padding of its own.
        anchors.topMargin: Theme.space.xl
        anchors.bottomMargin: Theme.space.lg
        width: Math.min(root.width - (Theme.space.xl + Theme.space.xs) * 2, root.readableWidth)
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("Minecraft name")
                maximumLength: 16
                selectByMouse: true
                Accessible.name: qsTr("Minecraft name of the friend to add")
                onAccepted: root.submit()
            }
            Button {
                highlighted: true
                enabled: root.nameValid && !root.sending
                text: qsTr("Add")
                icon.source: Icons.url("plus")
                onClicked: root.submit()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.typedName.length > 0 && !root.nameValid
            text: qsTr("A Minecraft name has 1 to 16 letters, numbers or underscores.")
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }

        TabStrip {
            Layout.fillWidth: true
            current: root.tab
            onActivated: (id) => root.tab = id
            tabs: [
                { id: "friends", label: qsTr("Friends"), count: root.controller ? root.controller.friends.length : 0 },
                { id: "requests", label: qsTr("Requests"), count: root.controller ? root.controller.incomingCount : 0 },
                { id: "blocked", label: qsTr("Blocked"), count: root.controller ? root.controller.blocked.length : 0 }
            ]
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            StackLayout {
                anchors.fill: parent
                currentIndex: root.tab === "requests" ? 1 : root.tab === "blocked" ? 2 : 0

                FriendsFriendsTab {
                    controller: root.controller
                    onRemoveRequested: (uuid, name) => root.confirmAction(true, uuid, name)
                    onBlockRequested: (uuid, name) => root.confirmAction(false, uuid, name)
                }
                FriendsRequestsTab {
                    controller: root.controller
                    onBlockRequested: (uuid, name) => root.confirmAction(false, uuid, name)
                }
                FriendsBlockedTab {
                    controller: root.controller
                }
            }
        }
    }

    // Friends off, signed out, connecting or failed: nothing to list.
    FriendsPageState {
        anchors.fill: parent
        visible: !root.connected
        controller: root.controller
        phase: root.phase
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // The answer to Add: only a request that went through empties the field.
    Connections {
        target: root.controller
        function onActionFinished(action, ok, message) {
            if (action !== "sendRequest")
                return
            root.sending = false
            if (ok)
                nameField.clear()
        }
    }

    // Removing a friend and blocking someone, both after a question.
    ConfirmDialog {
        id: confirm

        property bool removing: true
        property string targetUuid
        property string targetName

        title: confirm.removing ? qsTr("Remove %1?").arg(confirm.targetName) : qsTr("Block %1?").arg(confirm.targetName)
        confirmText: confirm.removing ? qsTr("Remove") : qsTr("Block")
        text: confirm.removing
              ? qsTr("%1 will no longer see when you are online, and you will not see them. You can add each other again later.").arg(confirm.targetName)
              : qsTr("%1 will be taken off your friends and can no longer send you requests or see when you are online. They are not told.").arg(confirm.targetName)
        onConfirmed: {
            if (confirm.removing)
                root.controller.removeFriend(confirm.targetUuid)
            else
                root.controller.block(confirm.targetUuid)
        }
    }
}
