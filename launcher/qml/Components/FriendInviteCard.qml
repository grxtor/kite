// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A friend's invite to a server, as Discord shows a call: their head, "LadyAgnes invited you", the server's favicon
 * and whole name, how long the invite lasts (counting down), a × that dismisses it and Join. It only says
 * what was asked for; FriendsWaiting joins (FriendJoinFlow.joinInvite()) or dismisses.
 */
Rectangle {
    id: root

    // FriendsController.invites entry: { uuid, name, face, host, port, expiresAt, addressKind }.
    property var invite: ({})
    // FriendPlaces.of() of the invite's server.
    property var place: ({})
    // The inviter's presence ("playing", "online", "offline" or "").
    property string presence: ""
    // The Main.qml FriendJoinFlow, for Join's tooltip.
    property var joinFlow: null
    property bool actionsEnabled: true
    // A dismiss is on its way.
    property bool busy: false
    // Ticks while shown, so the time left counts down.
    property double now: Date.now()
    readonly property int tickMs: 15 * 1000

    signal joinRequested()
    signal dismissRequested()

    readonly property int headSize: 32

    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.md
    color: Theme.palette.surface
    border.color: Theme.palette.border
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("%1 invited you to %2").arg(root.invite.name).arg(root.place.name || "")

    Timer {
        interval: root.tickMs
        repeat: true
        running: root.visible
        triggeredOnStart: true
        onTriggered: root.now = Date.now()
    }

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
                    // A long address loses its middle, never its end.
                    Copy {
                        Layout.fillWidth: true
                        text: root.place.name || ""
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        role: "label"
                        tone: "secondary"
                    }
                }
                // On a line of its own, so neither the name nor the place is cut for it.
                Copy {
                    width: parent.width
                    text: Format.inviteTimeLeft(root.invite, root.now)
                    role: "caption"
                    tone: "tertiary"
                    font.features: ({ "tnum": 1 })
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                size: Theme.control.heightSm
                iconName: "x"
                tip: qsTr("Dismiss")
                enabled: root.actionsEnabled && !root.busy
                onClicked: root.dismissRequested()
            }
        }

        Button {
            id: join
            Layout.fillWidth: true
            buttonSize: "sm"
            enabled: root.actionsEnabled
            text: qsTr("Join")
            icon.source: Icons.url("play")
            Accessible.name: qsTr("Join %1").arg(root.place.name || root.invite.host)
            // Worked out as the pointer arrives, so it is never an old answer.
            ToolTip.visible: join.hovered
            ToolTip.delay: 500
            ToolTip.text: join.hovered && root.joinFlow ? root.joinFlow.inviteTip(root.invite) : ""
            onClicked: root.joinRequested()
        }
    }
}
