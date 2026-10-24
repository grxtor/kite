// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The Friends dialog's "Requests" tab: people asking to be your friend
 * (accept, decline, or block them) above the requests you sent (cancel).
 */
Item {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null

    signal blockRequested(string uuid, string name)

    readonly property var rows: {
        var rows = []
        var incoming = root.controller ? root.controller.incoming : []
        var outgoing = root.controller ? root.controller.outgoing : []
        for (var i = 0; i < incoming.length; ++i)
            rows.push({ incoming: true, uuid: incoming[i].uuid, name: incoming[i].name, face: incoming[i].face })
        for (var j = 0; j < outgoing.length; ++j)
            rows.push({ incoming: false, uuid: outgoing[j].uuid, name: outgoing[j].name, face: outgoing[j].face })
        return rows
    }

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
            detail: modelData.incoming ? qsTr("Wants to be your friend") : qsTr("Waiting for them to answer")

            Button {
                visible: row.modelData.incoming
                buttonSize: "sm"
                text: qsTr("Accept")
                onClicked: root.controller.accept(row.modelData.uuid)
            }
            Button {
                visible: row.modelData.incoming
                buttonSize: "sm"
                text: qsTr("Decline")
                onClicked: root.controller.decline(row.modelData.uuid)
            }
            Button {
                visible: !row.modelData.incoming
                buttonSize: "sm"
                text: qsTr("Cancel")
                onClicked: root.controller.cancelRequest(row.modelData.uuid)
            }
            FriendMoreButton {
                visible: row.modelData.incoming
                canRemove: false
                onBlockRequested: root.blockRequested(row.modelData.uuid, row.modelData.name)
            }
        }
    }

    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: root.rows.length === 0
        title: qsTr("No friend requests")
        body: qsTr("Requests you get, and requests you send, wait here until they are answered.")
        MeshIcon { iconName: "user"; size: Theme.icon.lg + Theme.space.md; color: Theme.palette.textTertiary }
    }
}
