// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The top of a conversation: back to the friends list, the friend's head with presence, their name and where they
 * are (the friend rows' FriendPlaceLine: "Hypixel · 1.21.4" with its favicon), and ⋯ (the panel's FriendPersonMenu)
 * to remove or block them. A click on the head or name asks for their card (profileRequested). Someone no longer on
 * the list keeps their name here, without presence, card or ⋯.
 */
Item {
    id: root

    property var friend: ({})
    // They are still on the friends list.
    property bool known: true
    // The panel's FriendPersonMenu.
    property var menu: null
    // FriendPlaces.of(friend), or ({}).
    property var place: ({})
    property bool stale: false
    property bool actionsEnabled: true
    property color surface: Theme.palette.canvas
    // The card is open on this header.
    property bool cardOpen: false

    signal backRequested()
    // @p head: the FriendHead the card opens beside.
    signal profileRequested(var head)

    readonly property var status: Format.friendStatus(root.friend, root.place)

    implicitHeight: Theme.control.heightLg + Theme.space.md

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.sm
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.xs

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            size: Theme.control.heightSm
            iconName: "chevron-left"
            tip: qsTr("Back to friends")
            onClicked: root.backRequested()
        }

        AbstractButton {
            id: profile
            objectName: "chatProfile"
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: Theme.control.heightLg
            enabled: root.known
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.name: qsTr("%1's card").arg(root.friend.name || "")
            onClicked: root.profileRequested(head)

            HoverHandler { cursorShape: profile.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

            background: StateFill {
                card: true
                rest: "transparent"
                hovered: profile.hovered || profile.visualFocus || root.cardOpen
                radius: Theme.radius.md

                FocusFrame { around: 0; visible: profile.visualFocus }
            }
            contentItem: RowLayout {
                spacing: Theme.space.sm

                FriendHead {
                    id: head
                    Layout.leftMargin: Theme.space.xs
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
                    spacing: Theme.space.xxs

                    Copy {
                        width: parent.width
                        text: root.friend.name || ""
                        elide: Text.ElideRight
                        role: "bodyStrong"
                        Accessible.role: Accessible.Heading
                    }
                    Copy {
                        width: parent.width
                        visible: !root.known
                        text: qsTr("Not your friend any more")
                        elide: Text.ElideRight
                        role: "label"
                        tone: "tertiary"
                    }
                    FriendPlaceLine {
                        width: parent.width
                        visible: root.known && text.length > 0
                        text: root.status.text || ""
                        suffix: root.status.suffix || ""
                        tone: root.stale ? "faint" : root.status.tone
                        server: !!root.place.address
                        icon: root.place.favicon || ""
                    }
                }
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
