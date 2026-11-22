// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Adding a friend by Minecraft name, in the Friends page's toolbar: a field and Add (Enter does the same). Spaces
 * never get in (a pasted name often brings one, and would push a letter out of the 16), a name that cannot be one
 * says why under the field, and the request is on its way until FriendsController says how it went
 * (actionFinished): the name stays in the field unless it was accepted, and the cursor stays there either way.
 */
ColumnLayout {
    id: root

    // FriendsController, or null.
    property var controller: null
    // The friends server answers; without it a request cannot go.
    property bool online: true

    readonly property string typedName: nameField.text
    // Minecraft's own rule for a name.
    readonly property bool nameValid: /^[A-Za-z0-9_]{1,16}$/.test(root.typedName)
    readonly property bool ownName: !!root.controller && root.typedName.length > 0
                                    && root.typedName.toLowerCase() === String(root.controller.myName).toLowerCase()
    readonly property string problem: root.typedName.length === 0 ? ""
        : !root.nameValid ? qsTr("A Minecraft name has 1 to 16 letters, numbers or underscores.")
        : root.ownName ? qsTr("That is your own name.") : ""
    readonly property bool canSend: root.nameValid && !root.ownName && !root.sending && root.online
    property bool sending: false
    readonly property int fieldWidth: 232
    readonly property int maxNameLength: 16

    function focusField() {
        nameField.forceActiveFocus()
    }
    function submit() {
        if (!root.canSend || !root.controller)
            return
        root.sending = true
        root.controller.sendRequest(root.typedName)
    }

    spacing: Theme.space.xs

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.sm

        TextField {
            id: nameField
            objectName: "friendNameField"
            Layout.fillWidth: true
            Layout.preferredWidth: root.fieldWidth
            leftPadding: Theme.space.md + Theme.icon.sm + Theme.space.sm
            placeholderText: qsTr("Add a friend by name")
            selectByMouse: true
            enabled: root.online
            Accessible.name: qsTr("Minecraft name of the friend to add")
            // Cut to length here, after the spaces are out: a limit on the field would cut " Notch…" pasted whole
            // before them and lose the last letter.
            onTextEdited: {
                var clean = nameField.text.replace(/\s/g, "").slice(0, root.maxNameLength)
                if (clean !== nameField.text)
                    nameField.text = clean
            }
            onAccepted: root.submit()
            Keys.onEscapePressed: (event) => {
                event.accepted = nameField.text.length > 0
                nameField.clear()
            }

            MeshIcon {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space.md
                anchors.verticalCenter: parent.verticalCenter
                iconName: "user-plus"
                size: Theme.icon.sm
                color: nameField.activeFocus ? Theme.palette.textSecondary : Theme.palette.textTertiary
            }
        }
        Button {
            // The page's one filled button once there is a name to send.
            highlighted: root.canSend
            enabled: root.canSend
            text: root.sending ? qsTr("Sending…") : qsTr("Add")
            onClicked: root.submit()
        }
    }
    Copy {
        Layout.fillWidth: true
        Layout.preferredWidth: root.fieldWidth
        visible: root.problem.length > 0
        text: root.problem
        wrapMode: Text.Wrap
        role: "caption"
        tone: "danger"
    }

    Connections {
        target: root.controller
        function onActionFinished(action, ok, message) {
            if (action !== "sendRequest")
                return
            root.sending = false
            if (ok)
                nameField.clear()
            else
                nameField.selectAll()
            nameField.forceActiveFocus()
        }
        // Friends turned off and on while a request was out: its answer is dropped, so it is not waited for.
        function onChanged() {
            if (root.sending && root.controller.state !== "ready")
                root.sending = false
        }
    }
}
