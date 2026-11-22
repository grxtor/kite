// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Blocked" tab: people who can neither send you a request nor see when you are online, in one
 * card, each with an Unblock that waits for its answer (FriendsPending). `query` is the page's search.
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    property FriendsState friendsState: FriendsState { controller: root.controller }
    property bool actionsEnabled: true
    property string query

    signal clearRequested()

    readonly property var all: root.controller ? root.controller.blocked : []
    readonly property var blocked: root.friendsState.named(root.all, root.query)

    FriendsPending {
        id: pending
        controller: root.controller
        actions: ["unblock"]
    }

    FriendsListScroll {
        id: list
        anchors.fill: parent
        controller: root.controller
        count: root.blocked.length
        total: root.all.length
        query: root.query
        actionsEnabled: root.actionsEnabled
        onClearRequested: root.clearRequested()
        emptyState: EmptyState {
            upperThird: true
            title: qsTr("No one is blocked")
            body: qsTr("Someone you block cannot send you requests or see when you are online. They are not told.")
            iconName: "ban"
        }

        FriendsSectionHeader {
            width: parent.width
            text: qsTr("Blocked · %1").arg(root.blocked.length)
        }
        FriendsListCard {
            id: card
            width: parent.width

            Repeater {
                model: root.blocked
                delegate: FriendRow {
                    id: row
                    required property var modelData
                    required property int index
                    width: card.rowWidth
                    surface: card.color
                    divider: row.index > 0
                    face: modelData.face
                    name: modelData.name
                    detail: qsTr("Can't send you requests or see you online")
                    detailTone: "faint"

                    Button {
                        buttonSize: "sm"
                        enabled: !pending.has(row.modelData.uuid)
                        text: qsTr("Unblock")
                        Accessible.name: qsTr("Unblock %1").arg(row.modelData.name)
                        onClicked: pending.send(row.modelData.uuid, () => root.controller.unblock(row.modelData.uuid))
                    }
                }
            }
        }
    }
}
