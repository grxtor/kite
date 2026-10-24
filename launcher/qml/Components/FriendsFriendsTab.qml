// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends dialog's "Friends" tab: everyone on the list, those playing
 * first, with where they are and a ⋯ to remove or block them.
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    signal removeRequested(string uuid, string name)
    signal blockRequested(string uuid, string name)

    readonly property var rows: Format.friendsByPresence(root.controller ? root.controller.friends : [])

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
            uuid: modelData.uuid
            name: modelData.name
            detail: modelData.statusText
            presence: modelData.state
            detailStrong: modelData.worldType === "server" && !!modelData.host

            FriendMoreButton {
                onRemoveRequested: root.removeRequested(row.modelData.uuid, row.modelData.name)
                onBlockRequested: root.blockRequested(row.modelData.uuid, row.modelData.name)
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: root.rows.length === 0
        title: qsTr("No friends yet")
        body: qsTr("Add one by their Minecraft name. They need to have turned friends on in Kite too.")
        MeshIcon { iconName: "users"; size: Theme.icon.lg + Theme.space.md; color: Theme.palette.textTertiary }
    }
}
