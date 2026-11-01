// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Requests" tab: people asking to be your friend (Accept, Decline, or block from the ⋯) above the
 * requests you sent (marked as sent, with Cancel). A row's buttons wait for the answer to its click (FriendsPending).
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    property var menu: null
    property bool actionsEnabled: true

    readonly property var incoming: root.controller ? root.controller.incoming : []
    readonly property var outgoing: root.controller ? root.controller.outgoing : []

    FriendsPending {
        id: pending
        controller: root.controller
        actions: ["accept", "decline", "cancelRequest"]
    }

    FriendsListScroll {
        id: list
        anchors.fill: parent
        controller: root.controller
        count: root.incoming.length + root.outgoing.length
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.sm
        emptyState: EmptyState {
            upperThird: true
            title: qsTr("No friend requests")
            body: qsTr("Requests you get, and requests you send, wait here until they are answered.")
            iconName: "user"
        }

        Column {
            width: parent.width
            visible: root.incoming.length > 0
            spacing: Theme.space.xxs

            FriendsSectionHeader {
                width: parent.width
                text: qsTr("INCOMING — %1").arg(root.incoming.length)
            }
            Repeater {
                model: root.incoming
                delegate: FriendRow {
                    id: incomingRow
                    required property var modelData
                    width: list.width
                    face: modelData.face
                    name: modelData.name
                    detail: qsTr("Wants to be your friend")

                    Button {
                        buttonSize: "sm"
                        enabled: !pending.has(incomingRow.modelData.uuid)
                        text: qsTr("Accept")
                        icon.source: Icons.url("check")
                        onClicked: pending.send(incomingRow.modelData.uuid, () => root.controller.accept(incomingRow.modelData.uuid))
                    }
                    Button {
                        buttonSize: "sm"
                        variant: "ghost"
                        enabled: !pending.has(incomingRow.modelData.uuid)
                        text: qsTr("Decline")
                        icon.source: Icons.url("x")
                        onClicked: pending.send(incomingRow.modelData.uuid, () => root.controller.decline(incomingRow.modelData.uuid))
                    }
                    FriendMoreButton {
                        menu: root.menu
                        personUuid: incomingRow.modelData.uuid
                        personName: incomingRow.modelData.name
                        canRemove: false
                    }
                }
            }
        }

        Column {
            width: parent.width
            visible: root.outgoing.length > 0
            spacing: Theme.space.xxs

            FriendsSectionHeader {
                width: parent.width
                text: qsTr("SENT — %1").arg(root.outgoing.length)
            }
            Repeater {
                model: root.outgoing
                delegate: FriendRow {
                    id: sentRow
                    required property var modelData
                    width: list.width
                    face: modelData.face
                    name: modelData.name
                    detail: qsTr("Request sent")
                    detailTone: "faint"

                    Button {
                        buttonSize: "sm"
                        variant: "ghost"
                        enabled: !pending.has(sentRow.modelData.uuid)
                        text: qsTr("Cancel")
                        onClicked: pending.send(sentRow.modelData.uuid, () => root.controller.cancelRequest(sentRow.modelData.uuid))
                    }
                    // The ⋯ column the incoming rows have, so the buttons of both groups end on one line.
                    Item { width: Theme.control.heightSm; height: 1 }
                }
            }
        }
    }
}
