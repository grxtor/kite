// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Friends, but none around: the quiet kind of empty on Home's friends panel. A line saying so and the first few
 * offline friends as faded heads -- a tooltip names them on hover or keyboard focus, and a click opens the
 * Friends page (`clicked()`). Each head is a button so a screen reader reaches the names.
 */
Column {
    id: root

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

                background: Item {
                    FocusFrame { around: 0; cornerRadius: Theme.radius.sm; visible: head.visualFocus }
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
