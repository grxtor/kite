// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Blocked" tab: people who can neither send you a request nor see when you are online, each with
 * an Unblock that waits for its answer (FriendsPending).
 */
Item {
    id: root

    // FriendsController, or null.
    property var controller: null
    property bool actionsEnabled: true

    readonly property var blocked: root.controller ? root.controller.blocked : []

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
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.xxs
        emptyState: EmptyState {
            upperThird: true
            title: qsTr("No one is blocked")
            body: qsTr("Someone you block cannot send you requests or see when you are online. They are not told.")
            iconName: "ban"
        }

        FriendsSectionHeader {
            width: parent.width
            text: qsTr("BLOCKED — %1").arg(root.blocked.length)
        }
        Repeater {
            model: root.blocked
            delegate: FriendRow {
                id: row
                required property var modelData
                width: list.width
                face: modelData.face
                name: modelData.name

                Button {
                    buttonSize: "sm"
                    enabled: !pending.has(row.modelData.uuid)
                    text: qsTr("Unblock")
                    onClicked: pending.send(row.modelData.uuid, () => root.controller.unblock(row.modelData.uuid))
                }
                Item { width: Theme.control.heightSm; height: 1 }
            }
        }
    }
}
