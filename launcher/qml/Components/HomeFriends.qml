// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Friends" on Home's side panel, after "Set up Kite": which of your friends
 * are online and where. Off, it is a short offer to turn friends on (the
 * question is FriendsOptInDialog's); signed out, connecting or failed, a
 * line saying so; ready, the first few friends (those playing first) as
 * FriendRows, a row for waiting requests, and the + and Manage that open
 * the Friends dialog (Main.qml's, through manageRequested).
 */
Column {
    id: root

    // QmlShell.friends (FriendsController), or null where there is none.
    property var controller: null

    signal manageRequested(string tab)
    signal signInRequested()

    // Whether the section has anything to show; the panel hides without it.
    readonly property bool shown: !!root.controller
    // "off", "noAccount", "signingIn", "error" or "ready". The controller's
    // own "off" while it is enabled is only the moment before it connects.
    readonly property string phase: !root.controller ? ""
        : !root.controller.enabled ? "off"
        : root.controller.state === "off" ? "signingIn" : root.controller.state
    readonly property int maxRows: 6
    readonly property var ordered: Format.friendsByPresence(root.controller ? root.controller.friends : [])
    readonly property int incomingCount: root.controller ? root.controller.incomingCount : 0

    spacing: Theme.space.xs

    /*
     * Everything but "ready": the section's title, one line of what is going
     * on and, where there is one, the thing to press.
     */
    Rectangle {
        visible: root.phase !== "ready"
        width: parent.width
        implicitHeight: stateColumn.implicitHeight + Theme.space.md * 2
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        Column {
            id: stateColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space.md
            spacing: Theme.space.sm

            Text {
                width: parent.width
                text: qsTr("Friends")
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                lineHeight: Theme.type.label.lineHeight
                text: root.phase === "off" ? qsTr("See which server your friends are playing on.")
                    : root.phase === "noAccount" ? qsTr("Sign in with a Microsoft account to use friends.")
                    : root.phase === "error" ? (root.controller.error.length > 0 ? root.controller.error : qsTr("Friends could not connect."))
                    : qsTr("Connecting…")
            }
            Button {
                visible: root.phase === "off" || root.phase === "noAccount" || root.phase === "error"
                buttonSize: "sm"
                text: root.phase === "off" ? qsTr("Turn on")
                    : root.phase === "noAccount" ? qsTr("Sign in") : qsTr("Try again")
                onClicked: {
                    if (root.phase === "off")
                        optIn.open()
                    else if (root.phase === "noAccount")
                        root.signInRequested()
                    else
                        root.controller.refresh()
                }
            }
        }
    }

    // The ready state: heading, then the rows.
    RowLayout {
        visible: root.phase === "ready"
        width: parent.width
        spacing: Theme.space.sm

        HomePanelHeading {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: qsTr("Friends")
        }
        Text {
            visible: !!root.controller && root.controller.onlineCount > 0
            text: qsTr("%1 online").arg(root.controller ? root.controller.onlineCount : 0)
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
        IconButton {
            size: Theme.control.heightSm
            iconName: "plus"
            tip: qsTr("Add a friend")
            onClicked: root.manageRequested("friends")
        }
        LinkButton {
            text: qsTr("Manage")
            onClicked: root.manageRequested("friends")
        }
    }

    Text {
        visible: root.phase === "ready" && root.ordered.length === 0
        width: parent.width
        wrapMode: Text.Wrap
        text: qsTr("No friends yet. Add one by their Minecraft name.")
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        lineHeight: Theme.type.label.lineHeight
    }

    // Waiting requests come first: they want an answer.
    AbstractButton {
        id: requests
        visible: root.phase === "ready" && root.incomingCount > 0
        x: -Theme.space.sm
        width: parent.width + Theme.space.sm * 2
        height: Theme.control.height
        hoverEnabled: true
        leftPadding: Theme.space.sm
        rightPadding: Theme.space.sm
        text: root.incomingCount === 1 ? qsTr("1 friend request") : qsTr("%1 friend requests").arg(root.incomingCount)
        onClicked: root.manageRequested("requests")

        background: Rectangle {
            radius: Theme.radius.md
            color: requests.down ? Theme.palette.pressedOverlay
                 : requests.hovered ? Theme.palette.hoverOverlay : "transparent"
            border.width: requests.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        contentItem: RowLayout {
            Text {
                Layout.fillWidth: true
                text: requests.text
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.Medium
            }
            MeshIcon {
                iconName: "chevron-right"
                size: Theme.icon.sm
                color: Theme.palette.textSecondary
            }
        }
    }

    Repeater {
        model: root.phase === "ready" ? root.ordered.slice(0, root.maxRows) : []
        delegate: Item {
            id: slot
            required property var modelData
            width: root.width
            height: row.height

            // The head stands on the column's edge, with the heading; the
            // row's hover bleeds out by its own padding.
            FriendRow {
                id: row
                x: -Theme.space.sm
                width: root.width + Theme.space.sm * 2
                uuid: slot.modelData.uuid
                name: slot.modelData.name
                detail: slot.modelData.statusText
                presence: slot.modelData.state
                detailStrong: slot.modelData.worldType === "server" && !!slot.modelData.host
                interactive: true
                onClicked: root.manageRequested("friends")
            }
        }
    }

    FriendsOptInDialog {
        id: optIn
        controller: root.controller
    }
}
