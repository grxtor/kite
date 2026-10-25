// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Friends, but none of them around: the quiet kind of empty on Home's
 * friends panel. A line saying so and the first few offline friends as
 * faded heads -- who they are on hover or keyboard focus, and a click opens
 * the Friends page (`clicked()`), as the rows do when someone is around.
 * Each head is a button, so the keyboard and a screen reader reach the
 * names the tooltip carries.
 */
Column {
    id: root

    // Friend maps (FriendsController.friends) of the ones who are away.
    property var friends: []

    signal clicked()

    readonly property int maxHeads: 5
    readonly property int headSize: 24

    spacing: Theme.space.sm

    PanelNote {
        width: parent.width
        text: qsTr("No one's online right now.")
    }
    Row {
        spacing: Theme.space.xs

        Repeater {
            model: root.friends.slice(0, root.maxHeads)
            delegate: AbstractButton {
                id: head
                required property var modelData

                hoverEnabled: true
                padding: 0
                implicitWidth: root.headSize
                implicitHeight: root.headSize
                Accessible.name: head.modelData.name + ", " + head.modelData.statusText
                onClicked: root.clicked()

                HoverHandler { cursorShape: Qt.PointingHandCursor }
                ToolTip.visible: head.hovered || head.visualFocus
                ToolTip.delay: 500
                ToolTip.text: head.modelData.name + " · " + head.modelData.statusText

                background: Rectangle {
                    // The ring stands clear of the head, as other focus rings do.
                    anchors.margins: -Theme.focus.offset
                    radius: Theme.radius.sm
                    color: "transparent"
                    border.width: head.visualFocus ? Theme.focus.width : 0
                    border.color: Theme.palette.focusRing
                }
                contentItem: FriendHead {
                    size: root.headSize
                    face: head.modelData.face
                    presence: "offline"
                }
            }
        }
    }
}
