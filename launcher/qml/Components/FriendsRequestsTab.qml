// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Requests" tab: people asking to be your friend (Accept, Decline, or block from the ⋯) above the
 * requests you sent (Cancel), each group in its card. A row's buttons wait for the answer to its click
 * (FriendsPending). `query` is the page's search.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    property FriendsState friendsState: FriendsState { controller: root.controller }
    property var menu: null
    property bool actionsEnabled: true
    property string query

    signal clearRequested()

    readonly property var allIncoming: root.controller ? root.controller.incoming : []
    readonly property var allOutgoing: root.controller ? root.controller.outgoing : []
    readonly property var incoming: root.friendsState.named(root.allIncoming, root.query)
    readonly property var outgoing: root.friendsState.named(root.allOutgoing, root.query)

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
        total: root.allIncoming.length + root.allOutgoing.length
        query: root.query
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.sm
        onClearRequested: root.clearRequested()
        emptyState: EmptyState {
            upperThird: true
            title: qsTr("No friend requests")
            body: qsTr("Requests you get, and requests you send, wait here until they are answered.")
            iconName: "user"
        }

        Column {
            width: parent.width
            visible: root.incoming.length > 0

            FriendsSectionHeader {
                width: parent.width
                text: qsTr("Incoming · %1").arg(root.incoming.length)
            }
            FriendsListCard {
                id: incomingCard
                width: parent.width

                Repeater {
                    model: root.incoming
                    delegate: FriendRow {
                        id: incomingRow
                        required property var modelData
                        required property int index
                        width: incomingCard.rowWidth
                        surface: incomingCard.color
                        divider: incomingRow.index > 0
                        face: modelData.face
                        name: modelData.name
                        detail: qsTr("Wants to be your friend")
                        onMenuRequested: incomingMore.clicked()

                        Button {
                            buttonSize: "sm"
                            enabled: !pending.has(incomingRow.modelData.uuid)
                            text: qsTr("Accept")
                            icon.source: Icons.url("check")
                            Accessible.name: qsTr("Accept %1").arg(incomingRow.modelData.name)
                            onClicked: pending.send(incomingRow.modelData.uuid, () => root.controller.accept(incomingRow.modelData.uuid))
                        }
                        Button {
                            buttonSize: "sm"
                            enabled: !pending.has(incomingRow.modelData.uuid)
                            text: qsTr("Decline")
                            Accessible.name: qsTr("Decline %1").arg(incomingRow.modelData.name)
                            onClicked: pending.send(incomingRow.modelData.uuid, () => root.controller.decline(incomingRow.modelData.uuid))
                        }
                        FriendMoreButton {
                            id: incomingMore
                            menu: root.menu
                            personUuid: incomingRow.modelData.uuid
                            personName: incomingRow.modelData.name
                            canRemove: false
                        }
                    }
                }
            }
        }

        Column {
            width: parent.width
            visible: root.outgoing.length > 0

            FriendsSectionHeader {
                width: parent.width
                text: qsTr("Sent · %1").arg(root.outgoing.length)
            }
            FriendsListCard {
                id: sentCard
                width: parent.width

                Repeater {
                    model: root.outgoing
                    delegate: FriendRow {
                        id: sentRow
                        required property var modelData
                        required property int index
                        width: sentCard.rowWidth
                        surface: sentCard.color
                        divider: sentRow.index > 0
                        face: modelData.face
                        name: modelData.name
                        detail: qsTr("Waiting for their answer")
                        detailTone: "faint"

                        Button {
                            buttonSize: "sm"
                            enabled: !pending.has(sentRow.modelData.uuid)
                            text: qsTr("Cancel request")
                            Accessible.name: qsTr("Cancel the request to %1").arg(sentRow.modelData.name)
                            onClicked: pending.send(sentRow.modelData.uuid, () => root.controller.cancelRequest(sentRow.modelData.uuid))
                        }
                    }
                }
            }
        }
    }
}
