// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The card a click on a head in the friends strip opens: bigger head, name, where they are, Join and ⋯. One card
 * for the strip, since a card in a head would close under the pointer when the lists are rebuilt. A click on the
 * strip does not close it (the strip decides): a press that closed it first would reopen it on that head.
 */
Popup {
    id: root

    // FriendsController, or null.
    property var controller: null
    // The panel's FriendPersonMenu.
    property var menu: null
    property var joinFlow: null
    property bool stale: false
    property bool actionsEnabled: true
    property string friendUuid

    readonly property var friend: root.find(root.controller ? root.controller.friends : [], root.friendUuid)
    readonly property bool known: root.friend.uuid !== undefined
    readonly property var status: Format.friendStatus(root.friend)
    readonly property bool joinable: root.known && Format.friendJoinable(root.friend)

    readonly property int cardWidth: 232
    readonly property int headSize: 24

    signal joinRequested(var friend)

    function find(list, uuid) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].uuid === uuid)
                return list[i]
        }
        return ({})
    }

    // The head's centre in the strip's coordinates; the card stays centred on it as it grows.
    property real centreY: 0

    // Opens beside @p head (a FriendsStripHead); the popup keeps itself inside the window (`margins`).
    function openFor(head, uuid) {
        root.friendUuid = uuid
        root.centreY = head.mapToItem(root.parent, 0, head.height / 2).y
        root.open()
    }

    onKnownChanged: if (!root.known && root.opened) root.close()

    x: -root.width - Theme.space.sm
    y: Math.round(root.centreY - root.height / 2)
    width: root.cardWidth
    padding: Theme.space.sm
    margins: Theme.space.sm
    // Esc only reaches a popup that has the focus.
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    Connections {
        target: root.menu
        ignoreUnknownSignals: true
        function onRemoveRequested() { root.close() }
        function onBlockRequested() { root.close() }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.sm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

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
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    text: root.known ? Format.friendStatusLine(root.friend) : ""
                    elide: Text.ElideMiddle
                    color: root.status.tone === "faint" ? Theme.palette.textTertiary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.overline.pixelSize
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
            // Worked out as the pointer arrives, so it is never an old answer.
            ToolTip.visible: join.hovered
            ToolTip.delay: 500
            ToolTip.text: join.hovered && root.joinFlow && root.joinable ? root.joinFlow.tip(root.friend) : ""
            onClicked: {
                root.close()
                root.joinRequested(root.friend)
            }
        }

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
