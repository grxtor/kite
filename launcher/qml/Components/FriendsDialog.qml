// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The friends list in full: add someone by their Minecraft name, then three
 * tabs -- Friends (where each one is; ⋯ to remove or block), Requests
 * (accept, decline, cancel) and Blocked (unblock). Removing and blocking
 * ask first. What the server answered comes back as a toast from Main.qml,
 * off FriendsController.actionFinished.
 */
Dialog {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // "friends", "requests" or "blocked".
    property string tab: "friends"

    readonly property bool connected: !!root.controller && root.controller.enabled
                                      && root.controller.state === "ready"
    readonly property string typedName: nameField.text.trim()
    // Minecraft's own rule for a name.
    readonly property bool nameValid: /^[A-Za-z0-9_]{1,16}$/.test(root.typedName)
    readonly property int preferredWidth: 560
    readonly property int preferredHeight: 620
    readonly property int listMinHeight: 240

    function openTab(id) {
        root.tab = id
        root.open()
    }
    function submit() {
        if (!root.nameValid || !root.controller)
            return
        root.controller.sendRequest(root.typedName)
        nameField.clear()
    }
    function confirmAction(remove, uuid, name) {
        confirm.removing = remove
        confirm.targetUuid = uuid
        confirm.targetName = name
        confirm.open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.preferredHeight, parent ? parent.height - Theme.space.xxl * 2 : root.preferredHeight)
    modal: true
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Friends")

    header: DialogHeader {
        title: root.title
    }

    onOpened: {
        if (root.controller) {
            root.controller.refresh()
            // The Blocked tab's count, before it is ever opened.
            root.controller.refreshBlocked()
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            visible: root.connected
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
                enabled: root.nameValid
                text: qsTr("Add")
                icon.source: Icons.url("plus")
                onClicked: root.submit()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.connected && root.typedName.length > 0 && !root.nameValid
            text: qsTr("A Minecraft name has 1 to 16 letters, numbers or underscores.")
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }

        TabStrip {
            Layout.fillWidth: true
            visible: root.connected
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
            Layout.minimumHeight: root.listMinHeight
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            StackLayout {
                anchors.fill: parent
                visible: root.connected
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

            // Friends off, signed out or not connected: nothing to list.
            EmptyState {
                anchors.centerIn: parent
                width: parent.width
                visible: !root.connected
                title: qsTr("Friends are not connected")
                body: qsTr("Turn friends on from Home or in Settings, and sign in with a Microsoft account.")
                MeshIcon { iconName: "users"; size: Theme.icon.lg + Theme.space.md; color: Theme.palette.textTertiary }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        padding: Theme.space.xl
        topPadding: 0

        Button {
            text: qsTr("Close")
            onClicked: root.close()
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
