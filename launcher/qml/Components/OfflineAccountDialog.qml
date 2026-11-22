// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/* Asks for the name of a new offline account and adds it (AccountsController.addOffline). */
AppDialog {
    id: offlineDialog

    // AccountsController.
    property var controller: null

    size: "sm"
    closeOnPrimary: false
    title: qsTr("Add offline account")
    primaryText: qsTr("Add account")
    primaryEnabled: usernameField.text.trim().length > 0
    onPrimaryClicked: offlineDialog.submit()

    function submit() {
        if (offlineDialog.controller && offlineDialog.controller.addOffline(usernameField.text))
            offlineDialog.close()
        else
            offlineError.text = qsTr("Offline accounts need a Microsoft account that owns the game first, and a name no other offline account uses.")
    }

    onOpened: {
        usernameField.text = ""
        offlineError.text = ""
        usernameField.forceActiveFocus()
    }

    contentItem: Column {
        spacing: Theme.space.sm
        Copy {
            width: parent.width
            text: qsTr("The name shown in game. Offline accounts cannot join online-mode servers.")
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
        }
        TextField {
            id: usernameField
            width: parent.width
            placeholderText: qsTr("Username")
            selectByMouse: true
            onAccepted: if (offlineDialog.primaryEnabled) offlineDialog.submit()
        }
        Copy {
            id: offlineError
            width: parent.width
            visible: text.length > 0
            role: "caption"
            tone: "danger"
            wrapMode: Text.Wrap
        }
    }
}
