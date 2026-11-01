// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * "Create shortcut…": an icon on the desktop or in the applications list that starts one instance without opening
 * the launcher first, optionally joining a server and playing as a given account. The writing is
 * InstanceShortcutController's. `created(message)` cues a toast; a failure stays here with the reason.
 */
AppDialog {
    id: root

    // InstanceShortcutController.
    property var controller: null
    signal created(string message)

    property string location: ""
    property string failure: ""
    readonly property var locations: root.controller ? root.controller.locations : []
    readonly property var accounts: root.controller ? root.controller.accounts : []
    readonly property string server: serverBox.checked ? serverField.text.trim() : ""
    readonly property bool complete: !serverBox.checked || root.server.length > 0

    function create(where) {
        var account = accountBox.checked ? accountPicker.currentText : ""
        if (!root.controller.create(nameField.text, where, root.server, account)) {
            root.failure = root.controller.error
            return
        }
        var message = root.location === "desktop" ? qsTr("Shortcut added to your desktop.")
                    : root.location === "applications" ? qsTr("Shortcut added to Applications.")
                    : qsTr("Shortcut created.")
        root.close()
        root.created(message)
    }

    size: "sm"
    closeOnPrimary: false
    title: qsTr("Create shortcut")
    primaryText: root.location === "other" ? qsTr("Create…") : qsTr("Create")
    primaryEnabled: !!root.controller && root.complete
    onPrimaryClicked: {
        root.failure = ""
        if (root.location !== "other") {
            root.create(root.location)
            return
        }
        var name = nameField.text.trim().length > 0 ? nameField.text.trim() : nameField.placeholderText
        saveDialog.selectedFile = root.controller.suggestedFileUrl(name)
        saveDialog.open()
    }

    onOpened: {
        root.failure = ""
        root.location = root.locations.length > 0 ? root.locations[0].value : "other"
        nameField.text = ""
        serverBox.checked = false
        serverField.text = ""
        accountBox.checked = false
        var preferred = root.controller ? root.accounts.indexOf(root.controller.defaultAccount) : -1
        accountPicker.currentIndex = Math.max(0, preferred)
        nameField.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: root.controller ? qsTr("Starts “%1” straight away, without opening Kite first.").arg(root.controller.instanceName) : ""
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Name") }
            TextField {
                id: nameField
                Layout.fillWidth: true
                // Follows the server until the user types a name.
                placeholderText: root.controller ? root.controller.suggestedName(root.server) : ""
                selectByMouse: true
                onAccepted: if (root.complete && root.location !== "other") root.create(root.location)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Place it in") }
            ComboBox {
                Layout.fillWidth: true
                // "Other" is the one that asks for a folder afterwards.
                model: root.locations.map((l) => ({
                    value: l.value,
                    label: l.value === "other" ? qsTr("Choose folder…") : l.label
                }))
                textRole: "label"
                valueRole: "value"
                currentIndex: indexOfValue(root.location)
                onActivated: root.location = currentValue
                Accessible.name: qsTr("Place it in")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs
            CheckBox {
                id: serverBox
                leftPadding: 0
                text: qsTr("Join a server when it starts")
            }
            TextField {
                id: serverField
                Layout.fillWidth: true
                visible: serverBox.checked
                placeholderText: qsTr("Server address, for example play.example.net")
                selectByMouse: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.accounts.length > 1
            spacing: Theme.space.xs
            CheckBox {
                id: accountBox
                leftPadding: 0
                text: qsTr("Play as a different account")
            }
            ComboBox {
                id: accountPicker
                Layout.fillWidth: true
                visible: accountBox.checked
                model: root.accounts
            }
        }

        Copy {
            Layout.fillWidth: true
            text: qsTr("Deleting the instance deletes its shortcuts too, as long as they stay where they were made.")
            role: "caption"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }

        Copy {
            Layout.fillWidth: true
            visible: root.failure.length > 0
            text: root.failure
            role: "caption"
            tone: "danger"
            wrapMode: Text.Wrap
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save shortcut")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.controller && root.controller.fileSuffix.length > 0 ? root.controller.fileSuffix.slice(1) : ""
        onAccepted: root.create(String(selectedFile))
    }
}
