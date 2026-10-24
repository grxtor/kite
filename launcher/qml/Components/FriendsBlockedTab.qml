// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends page's "Blocked" tab: people who can neither send you a
 * request nor see when you are online, each with an Unblock.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    readonly property var rows: root.controller ? root.controller.blocked : []

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.space.xs
        clip: true
        model: root.rows
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: FriendRow {
            id: row
            required property var modelData
            width: list.width
            face: modelData.face
            name: modelData.name

            Button {
                buttonSize: "sm"
                text: qsTr("Unblock")
                onClicked: root.controller.unblock(row.modelData.uuid)
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: root.rows.length === 0
        title: qsTr("No one is blocked")
        body: qsTr("Someone you block cannot send you requests or see when you are online. They are not told.")
        MeshIcon { iconName: "ban"; size: Theme.icon.lg + Theme.space.md; color: Theme.palette.textTertiary }
    }
}
