// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Add a server…" from the quick-join bar's ⋯ menu: an address and, if wanted, a name. Add pings the address first;
 * a server that answers joins the bar at once (QuickJoinLayout.pin()). One that does not is said so, and a second
 * press adds it anyway, as a server can be down for the night.
 */
AppDialog {
    id: root

    // QuickJoinLayout and ServerPinger, or null.
    property var layout: null
    property var pinger: null

    // The address Add asked the pinger about; "" until then, and again once the address is edited.
    property string asked: ""
    readonly property string address: addressField.text.trim()
    readonly property var status: root.statusOf(root.asked, root.pinger ? root.pinger.revision : 0)
    readonly property string answer: root.asked.length > 0 && root.status && root.status.state ? root.status.state : ""
    readonly property bool waiting: root.answer === "pending" || (root.asked.length > 0 && root.answer.length === 0)
    readonly property bool unusable: root.address.length > 0 && !!root.layout && !root.layout.isAddress(root.address)

    signal added(string name)

    function statusOf(address, revision) {
        return root.pinger && address.length > 0 ? root.pinger.status(address) : ({})
    }
    function start() {
        nameField.text = ""
        addressField.text = ""
        root.asked = ""
        root.open()
        addressField.forceActiveFocus()
    }
    function submit() {
        if (root.address.length === 0 || root.unusable || root.waiting)
            return
        if (root.answer === "offline" || !root.pinger) {
            root.add()
            return
        }
        root.asked = root.address
        // A fresh answer from the last minute will do.
        root.pinger.ping(root.address)
    }
    function add() {
        if (!root.layout || !root.layout.pin(nameField.text, root.address))
            return
        root.added(nameField.text.trim().length > 0 ? nameField.text.trim() : root.address)
        root.close()
    }

    onAnswerChanged: if (root.answer === "online") root.add()

    size: "sm"
    title: qsTr("Add a server")
    primaryText: root.answer === "offline" ? qsTr("Add anyway") : qsTr("Add")
    primaryIcon: "plus"
    primaryEnabled: root.address.length > 0 && !root.unusable && !root.waiting
    closeOnPrimary: false
    onPrimaryClicked: root.submit()

    contentItem: Column {
        spacing: Theme.space.sm

        FieldLabel { text: qsTr("Address") }
        TextField {
            id: addressField
            objectName: "quickJoinAddAddress"
            width: parent.width
            placeholderText: qsTr("play.example.com")
            selectByMouse: true
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
            onTextEdited: root.asked = ""
            onAccepted: root.submit()
        }

        Item { width: 1; height: Theme.space.xs }
        FieldLabel { text: qsTr("Name (optional)") }
        TextField {
            id: nameField
            objectName: "quickJoinAddName"
            width: parent.width
            placeholderText: root.address.length > 0 ? root.address : qsTr("My server")
            selectByMouse: true
            onAccepted: root.submit()
        }

        Item { width: 1; height: Theme.space.xs }
        Copy {
            objectName: "quickJoinAddState"
            width: parent.width
            visible: text.length > 0
            text: root.unusable ? qsTr("That is not a server address.")
                : root.waiting ? qsTr("Asking the server…")
                : root.answer === "offline" ? qsTr("This server is not answering. Check the address, or add it anyway.")
                : ""
            role: "label"
            tone: root.unusable || root.answer === "offline" ? "warning" : "secondary"
            wrapMode: Text.Wrap
        }
    }
}
