// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Message jeb_" at the foot of a friend's card, like Discord's: it looks like the field you will write in, a click
 * opens the conversation in the friends panel. Their unread messages are counted at its end.
 */
AbstractButton {
    id: root

    property string name
    property int unread: 0

    // As tall as Join above it.
    implicitHeight: Theme.control.heightSm
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: qsTr("Message %1").arg(root.name)
    Accessible.description: Format.unreadMessages(root.unread)

    background: Rectangle {
        radius: Theme.radius.sm
        color: Theme.palette.surfaceSunken
        border.color: root.hovered ? Theme.neutral.borderHover : Theme.palette.border

        Behavior on border.color {
            ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        }

        FocusFrame { around: 0; cornerRadius: Theme.radius.sm; visible: root.visualFocus }
    }
    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.sm

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "message-circle"
                size: Theme.icon.xs
                color: Theme.palette.textTertiary
            }
            Copy {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Message %1").arg(root.name)
                role: "label"
                tone: "tertiary"
            }
        }
        CountPill {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            count: root.unread
            ringColor: Theme.palette.surfaceSunken
        }
    }
}
