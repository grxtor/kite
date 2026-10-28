// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What a click on a head in the friends strip (FriendsStripHead) opens: a
 * small card at its left with the bigger head, the name and where they are,
 * Join when they let you, and the ⋯ that removes or blocks them (the
 * panel's FriendPersonMenu, `menu`). It is the narrow strip's version of a
 * row of the full panel, so it says no more than a row does.
 *
 * One card for the strip, not one per head: the lists are rebuilt whenever
 * they are read again, and a card in a head would close under the pointer.
 * It holds on to who it is for by uuid and reads that friend off the
 * controller each time, so a poll that moves them (online, then in game)
 * updates the card; it closes when they are no longer on the list.
 *
 * Esc, a click outside it, and choosing from ⋯ (whose question is the
 * panel's FriendConfirmDialog) all close it.
 */
Popup {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // The panel's FriendPersonMenu.
    property var menu: null
    // Main.qml's FriendJoinFlow, which says what Join would start.
    property var joinFlow: null
    // The list is the last one read, not a live one.
    property bool stale: false
    // The server answers: Join and ⋯ need it.
    property bool actionsEnabled: true
    property string friendUuid

    // The friend's map, off the controller's current list; empty once they
    // are gone.
    readonly property var friend: root.find(root.controller ? root.controller.friends : [], root.friendUuid)
    readonly property bool known: root.friend.uuid !== undefined
    readonly property var status: Format.friendStatus(root.friend)
    readonly property bool joinable: root.known && Format.friendJoinable(root.friend)

    readonly property int cardWidth: 296
    // The big head: a multiple of 8, like every head.
    readonly property int headSize: 56

    signal joinRequested(var friend)

    function find(list, uuid) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].uuid === uuid)
                return list[i]
        }
        return ({})
    }

    // Where the head's centre is, in the strip's coordinates; the card stays
    // centred on it as it grows (a friend who becomes joinable gets a Join).
    property real centreY: 0

    // Opens beside @p head (a FriendsStripHead), at the strip's left. The
    // popup keeps itself inside the window (`margins`), so the first and
    // last heads' cards slide to fit rather than run off it.
    function openFor(head, uuid) {
        root.friendUuid = uuid
        root.centreY = head.mapToItem(root.parent, 0, head.height / 2).y
        root.open()
    }

    onKnownChanged: if (!root.known && root.opened) root.close()

    x: -root.width - Theme.space.sm
    y: Math.round(root.centreY - root.height / 2)
    width: root.cardWidth
    padding: Theme.space.md
    margins: Theme.space.sm
    // Esc only reaches a popup that has the focus.
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Connections {
        target: root.menu
        ignoreUnknownSignals: true
        function onRemoveRequested() { root.close() }
        function onBlockRequested() { root.close() }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            FriendHead {
                Layout.alignment: Qt.AlignVCenter
                size: root.headSize
                face: root.known ? root.friend.face : ""
                presence: !root.known ? ""
                        : root.stale && root.friend.state !== "offline" ? "" : root.friend.state
                ringColor: Theme.glass.popup
            }

            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xxs

                Text {
                    width: parent.width
                    text: root.known ? root.friend.name : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }
                Text {
                    width: parent.width
                    text: root.known ? Format.friendStatusLine(root.friend) : ""
                    elide: Text.ElideMiddle
                    color: root.status.tone === "faint" ? Theme.palette.textTertiary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }

            FriendMoreButton {
                Layout.alignment: Qt.AlignTop
                menu: root.menu
                personUuid: root.friendUuid
                personName: root.known ? root.friend.name : ""
                enabled: root.actionsEnabled
            }
        }

        Button {
            id: join
            Layout.fillWidth: true
            visible: root.joinable
            enabled: root.actionsEnabled
            buttonSize: "sm"
            text: qsTr("Join")
            Accessible.name: root.known ? qsTr("Join %1 on %2").arg(root.friend.name).arg(root.friend.host) : ""
            // What it does: it starts the game, with the instance named.
            // Worked out as the pointer arrives, so it is never an old answer.
            ToolTip.visible: join.hovered
            ToolTip.delay: 500
            ToolTip.text: join.hovered && root.joinFlow && root.joinable ? root.joinFlow.tip(root.friend) : ""
            onClicked: {
                root.close()
                root.joinRequested(root.friend)
            }
        }

        // A visible server whose player turned joining off: said here, where
        // a Join would be, instead of looking like a bug.
        Text {
            Layout.fillWidth: true
            visible: root.known && Format.friendJoinOff(root.friend)
            text: root.known ? qsTr("%1 doesn't let friends join").arg(root.friend.name) : ""
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
    }
}
