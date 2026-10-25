// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Blocked" tab: people who can neither send you a
 * request nor see when you are online, each with an Unblock that waits for
 * its answer (FriendsPending).
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // The server answers: Unblock needs it, reading the list does not.
    property bool actionsEnabled: true

    readonly property var blocked: root.controller ? root.controller.blocked : []
    // Not before the lists have been read: an empty list that is still
    // arriving is not "no one".
    readonly property bool empty: !!root.controller && root.controller.loaded && root.blocked.length === 0

    FriendsPending {
        id: pending
        controller: root.controller
        actions: ["unblock"]
    }

    FriendsListScroll {
        id: list
        anchors.fill: parent
        visible: root.blocked.length > 0
        actionsEnabled: root.actionsEnabled
        spacing: Theme.space.xxs

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
                // The ⋯ column the Requests tab's rows have.
                Item { width: Theme.control.heightSm; height: 1 }
            }
        }
    }

    EmptyState {
        anchors.fill: parent
        visible: root.empty
        upperThird: true
        title: qsTr("No one is blocked")
        body: qsTr("Someone you block cannot send you requests or see when you are online. They are not told.")
        SoftIconCircle { iconName: "ban" }
    }
}
