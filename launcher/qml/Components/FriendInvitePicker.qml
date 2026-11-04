// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Invite friends to <server>", as Discord's invite list: a search field, then every friend (in game, in Kite,
 * offline) with Invite at the end of the row. A friend already invited there reads "Invited" until the invite runs
 * out. Main.qml holds one; the Servers tab's and the quick-join bar's ⋯ menus open it with openFor({ name, address }).
 */
AppDialog {
    id: root

    // FriendsController, or null.
    property var controller: null
    property FriendPlaces places: FriendPlaces {}
    // The server: { name, address }.
    property var server: ({})
    // The friend whose invite is on its way, "" when none: one at a time, so the answer is known to be theirs.
    property string sendingUuid: ""

    readonly property FriendsState friendsState: FriendsState { controller: root.controller }
    readonly property string query: search.text.trim().toLowerCase()
    readonly property var everyone: {
        var groups = root.friendsState.groups
        var all = groups.playing.concat(groups.online, groups.offline)
        return root.query.length > 0 ? all.filter((one) => one.name.toLowerCase().indexOf(root.query) >= 0) : all
    }
    readonly property int listRows: 6

    function openFor(server) {
        root.server = server
        root.sendingUuid = ""
        search.text = ""
        root.open()
        search.forceActiveFocus()
    }
    // "mc.hypixel.net", without the default port, as an invite's address reads.
    function plainAddress(address) {
        var lower = String(address || "").trim().toLowerCase().replace(/\.$/, "")
        return lower.endsWith(":" + Format.defaultServerPort) ? lower.slice(0, lower.lastIndexOf(":")) : lower
    }
    function invitedHere(uuid) {
        var sent = root.controller ? root.controller.outgoingInvites.find((one) => one.uuid === uuid) : null
        return !!sent && Format.friendAddress(sent) === root.plainAddress(root.server.address)
    }
    function invite(uuid) {
        root.sendingUuid = uuid
        root.controller.sendInvite(uuid, root.server.address)
    }

    size: "md"
    closeOnOutsidePress: true
    title: qsTr("Invite friends to %1").arg(root.server.name || root.server.address || "")
    showCancel: false
    primaryText: qsTr("Done")

    Connections {
        target: root.controller
        function onActionFinished(action) {
            if (action === "sendInvite")
                root.sendingUuid = ""
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        SearchBox {
            id: search
            Layout.fillWidth: true
            visible: root.friendsState.connected && root.friendsState.friendCount > 0
            placeholderText: qsTr("Search friends")
        }

        Copy {
            Layout.fillWidth: true
            visible: !root.friendsState.connected || root.everyone.length === 0
            text: !root.friendsState.connected ? qsTr("Friends isn't connected. Invites go out once it is.")
                : root.friendsState.friendCount === 0 ? qsTr("No friends yet. Add them by their Minecraft name in Friends.")
                : qsTr("No friend is called that.")
            tone: "secondary"
            wrapMode: Text.Wrap
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(list.contentHeight, root.listRows * (Theme.control.height + Theme.space.md))
            visible: root.friendsState.connected && root.everyone.length > 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.everyone
            ScrollBar.vertical: ScrollBar {}

            delegate: FriendStatusRow {
                id: row
                required property var modelData
                readonly property bool invited: root.invitedHere(row.modelData.uuid)
                readonly property bool sending: root.sendingUuid === row.modelData.uuid

                width: list.width
                compact: true
                friend: row.modelData
                place: root.places.of(row.modelData)

                Button {
                    anchors.verticalCenter: parent.verticalCenter
                    buttonSize: "sm"
                    enabled: !row.invited && root.sendingUuid.length === 0
                    text: row.sending ? qsTr("Inviting…") : row.invited ? qsTr("Invited") : qsTr("Invite")
                    icon.source: row.invited ? Icons.url("check") : ""
                    Accessible.name: qsTr("Invite %1").arg(row.modelData.name)
                    onClicked: root.invite(row.modelData.uuid)
                }
            }
        }
    }
}
