// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * A friend's invite to a server, as Discord shows a call: their head, "LadyAgnes invited you", the server's favicon
 * and name with how long the invite lasts, a × that dismisses it and Join under it. It only says what was asked
 * for; FriendsWaiting joins or dismisses.
 */
Rectangle {
    id: root

    // { uuid, name, face, host, port, expiresAt }.
    property var invite: ({})
    // What Join joins (FriendsWaiting.joinTarget()): the invite's server with the inviter's game version.
    property var target: ({})
    // FriendPlaces.of(target).
    property var place: ({})
    // The inviter's presence ("playing", "online", "offline" or "").
    property string presence: ""
    property var joinFlow: null
    property bool actionsEnabled: true

    signal joinRequested()
    signal dismissRequested()

    readonly property int headSize: 32

    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.md
    color: Theme.palette.surface
    border.color: Theme.palette.border
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1 invited you to %2").arg(root.invite.name).arg(root.place.name || "")

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.space.md
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            FriendHead {
                Layout.alignment: Qt.AlignVCenter
                size: root.headSize
                face: root.invite.face || ""
                presence: root.presence
                ringColor: root.color
            }
            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xxs

                Copy {
                    width: parent.width
                    text: qsTr("%1 invited you").arg(root.invite.name)
                    elide: Text.ElideRight
                    role: "label"
                    font.weight: Font.DemiBold
                }
                RowLayout {
                    width: parent.width
                    spacing: Theme.space.xs

                    ServerFavicon {
                        Layout.alignment: Qt.AlignVCenter
                        source: root.place.favicon || ""
                    }
                    // The server's name gives way before the time left does.
                    Copy {
                        Layout.fillWidth: true
                        Layout.maximumWidth: implicitWidth
                        text: root.place.name || ""
                        elide: Text.ElideRight
                        role: "label"
                        tone: "secondary"
                    }
                    Copy {
                        text: "· " + Format.inviteTimeLeft(root.invite)
                        role: "label"
                        tone: "secondary"
                    }
                    Item { Layout.fillWidth: true }
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                size: Theme.control.heightSm
                iconName: "x"
                tip: qsTr("Dismiss")
                enabled: root.actionsEnabled
                onClicked: root.dismissRequested()
            }
        }

        FriendJoinButton {
            Layout.fillWidth: true
            enabled: root.actionsEnabled
            friend: root.target
            joinFlow: root.joinFlow
            icon.source: Icons.url("play")
            onClicked: root.joinRequested()
        }
    }
}
