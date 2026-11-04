// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Message jeb_" at the foot of a friend's card, where a chat with them will open, with their unread messages
 * counted at its end. The launcher has no chat yet, so it looks disabled and its tip says "Coming soon"; it stays
 * enabled only so the tip can show.
 */
AbstractButton {
    id: root

    property string name
    property int unread: 0

    readonly property int tipDelayMs: 500

    implicitHeight: Theme.control.height
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: qsTr("Message %1").arg(root.name)
    Accessible.description: [qsTr("Coming soon"), Format.unreadMessages(root.unread)]
                            .filter((part) => part.length > 0).join(", ")

    ToolTip.visible: root.hovered || root.visualFocus
    ToolTip.delay: root.tipDelayMs
    ToolTip.text: qsTr("Coming soon")

    background: Rectangle {
        radius: Theme.radius.sm
        color: Theme.palette.surfaceSunken
        border.color: Theme.palette.border

        FocusFrame { around: 0; cornerRadius: Theme.radius.sm; visible: root.visualFocus }
    }
    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.sm
            opacity: Theme.opacity.disabled

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
