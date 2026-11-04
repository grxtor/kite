// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The top of a conversation: back to the friends list, the friend's head with presence, their name and where they
 * are, and ⋯ (the panel's FriendPersonMenu) to remove or block them. Someone no longer on the list keeps their name
 * here, without presence or ⋯.
 */
Item {
    id: root

    property var friend: ({})
    // They are still on the friends list.
    property bool known: true
    // The panel's FriendPersonMenu.
    property var menu: null
    property bool stale: false
    property bool actionsEnabled: true
    property color surface: Theme.palette.canvas

    signal backRequested()

    readonly property var presence: Format.friendPresence(root.friend)

    implicitHeight: Theme.control.heightLg + Theme.space.md

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.sm
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.sm

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            size: Theme.control.heightSm
            iconName: "chevron-left"
            tip: qsTr("Back to friends")
            onClicked: root.backRequested()
        }
        FriendHead {
            Layout.alignment: Qt.AlignVCenter
            size: 32
            face: root.friend.face || ""
            presence: !root.known ? ""
                    : root.stale && root.friend.state !== "offline" ? "" : (root.friend.state || "")
            ringColor: root.surface
        }
        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter

            Copy {
                width: parent.width
                text: root.friend.name || ""
                elide: Text.ElideRight
                role: "bodyStrong"
                Accessible.role: Accessible.Heading
            }
            Copy {
                width: parent.width
                text: !root.known ? qsTr("Not your friend any more") : root.presence.text
                elide: Text.ElideRight
                role: "caption"
                tone: !root.known || root.stale ? "tertiary" : root.presence.tone
            }
        }
        FriendMoreButton {
            Layout.alignment: Qt.AlignVCenter
            visible: root.known
            enabled: root.actionsEnabled
            menu: root.menu
            personUuid: root.friend.uuid || ""
            personName: root.friend.name || ""
            canMessage: false
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.palette.divider
    }
}
