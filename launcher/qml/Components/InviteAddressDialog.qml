// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * Before an invite to a raw IP address or an address on a home network is joined: the address as it is, who sent
 * it and why it is asked, then Join or Cancel. FriendJoinFlow opens it with ask(invite, server); confirmed() hands
 * back the server to join.
 */
AppDialog {
    id: root

    property var invite: ({})
    // ServerJoinFlow's server for the invite.
    property var server: ({})
    readonly property bool local: root.invite.addressKind === "local"

    signal confirmed()

    function ask(invite, server) {
        root.invite = invite
        root.server = server
        root.open()
    }

    size: "sm"
    closeOnOutsidePress: true
    title: root.local ? qsTr("Join a server on a home network?") : qsTr("Join a server by its IP address?")
    primaryText: qsTr("Join")
    primaryIcon: "play"
    // Closed first, so joining can open the instance picker.
    closeOnPrimary: false
    onPrimaryClicked: {
        root.close()
        root.confirmed()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: qsTr("%1 invited you to:").arg(root.invite.name || "")
            tone: "secondary"
            wrapMode: Text.Wrap
        }
        BoxSurface {
            Layout.fillWidth: true
            implicitHeight: address.implicitHeight + Theme.space.md * 2
            radius: Theme.radius.md

            Copy {
                id: address
                anchors.fill: parent
                anchors.margins: Theme.space.md
                verticalAlignment: Text.AlignVCenter
                text: root.server.address || ""
                textFormat: Text.PlainText
                font.family: Theme.font.mono
                elide: Text.ElideMiddle
            }
        }
        Copy {
            Layout.fillWidth: true
            text: root.local
                  ? qsTr("This address is on a home or local network. It may only work on theirs, and Kite can't tell whose server it is.")
                  : qsTr("This is a raw IP address, not a server name, so Kite can't tell whose server it is.")
            tone: "secondary"
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }
        Copy {
            Layout.fillWidth: true
            text: qsTr("Join only if you trust %1.").arg(root.invite.name || "")
            tone: "secondary"
            wrapMode: Text.Wrap
        }
    }
}
