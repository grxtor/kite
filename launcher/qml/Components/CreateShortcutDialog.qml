// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * "Create shortcut…": an icon on the desktop or in the applications list
 * that starts one instance without opening the launcher first. The QML
 * replacement for CreateShortcutDialog; the writing is
 * InstanceShortcutController's, over the same core code the widget dialog
 * uses.
 *
 * "Choose folder…" asks where with a save dialog. The shortcut can also join a
 * server and play as a given account. `created(message)` is the page's
 * cue for a toast; a failure stays here with the reason.
 */
Dialog {
    id: root

    // InstanceShortcutController (QmlShell's shortcutController()).
    property var controller: null
    signal created(string message)

    property string location: ""
    property string failure: ""
    readonly property var locations: root.controller ? root.controller.locations : []
    readonly property var accounts: root.controller ? root.controller.accounts : []
    readonly property string server: serverBox.checked ? serverField.text.trim() : ""
    readonly property bool complete: !serverBox.checked || root.server.length > 0

    readonly property int dialogWidth: 480

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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.dialogWidth, parent ? parent.width - Theme.space.xxl * 2 : root.dialogWidth)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Create shortcut")

    header: DialogHeader {
        title: root.title
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

        Text {
            Layout.fillWidth: true
            text: root.controller ? qsTr("Starts “%1” straight away, without opening MeshMC first.").arg(root.controller.instanceName) : ""
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
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

        // Choosing among one account is no choice.
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

        Text {
            Layout.fillWidth: true
            text: qsTr("Deleting the instance deletes its shortcuts too, as long as they stay where they were made.")
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }

        Text {
            Layout.fillWidth: true
            visible: root.failure.length > 0
            text: root.failure
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            highlighted: true
            enabled: !!root.controller && root.complete
            text: root.location === "other" ? qsTr("Create…") : qsTr("Create")
            onClicked: {
                root.failure = ""
                if (root.location !== "other") {
                    root.create(root.location)
                    return
                }
                var name = nameField.text.trim().length > 0 ? nameField.text.trim() : nameField.placeholderText
                saveDialog.selectedFile = root.controller.suggestedFileUrl(name)
                saveDialog.open()
            }
        }
        Button {
            flat: true
            text: qsTr("Cancel")
            onClicked: root.close()
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
