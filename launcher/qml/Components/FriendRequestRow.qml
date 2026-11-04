// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Someone asking to be your friend, in a friends list: head, name, "Wants to be your friend", ✓ to accept and × to
 * decline. Blocking stays on the Friends page's ⋯. It only says what was asked for; `busy` while an answer is out.
 */
FriendRow {
    id: root

    // { uuid, name, face }.
    property var person: ({})
    property bool busy: false
    property bool actionsEnabled: true

    signal acceptRequested()
    signal declineRequested()

    face: root.person.face || ""
    name: root.person.name || ""
    detail: qsTr("Wants to be your friend")
    compact: true

    IconButton {
        size: Theme.control.heightSm
        iconName: "check"
        tip: qsTr("Accept")
        Accessible.name: qsTr("Accept %1").arg(root.name)
        enabled: root.actionsEnabled && !root.busy
        onClicked: root.acceptRequested()
    }
    IconButton {
        size: Theme.control.heightSm
        iconName: "x"
        tip: qsTr("Decline")
        Accessible.name: qsTr("Decline %1").arg(root.name)
        enabled: root.actionsEnabled && !root.busy
        onClicked: root.declineRequested()
    }
}
