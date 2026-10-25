// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * You, at the top of the Friends page: your head and name (a click copies
 * it, to send to someone), what your friends can see of you right now --
 * online, the server you play on and whether they can join you there
 * (Settings > Friends) -- and, on the right, where you add someone by their
 * Minecraft name. In a narrow window the add field drops under your name.
 *
 * A request is on its way until FriendsController says how it went
 * (actionFinished); the name stays in the field until it is accepted, so a
 * failed one is not typed again.
 */
Rectangle {
    id: root

    // QmlShell.friends (FriendsController), or null.
    property var controller: null
    // Your head (QmlShell.accountFace), "" for none.
    property string face: ""
    // The friends server answers: without it a request cannot go and the
    // status below is not known to have reached anyone (the page keeps the
    // last lists while it does not).
    property bool online: true

    readonly property string typedName: nameField.text.trim()
    // Minecraft's own rule for a name.
    readonly property bool nameValid: /^[A-Za-z0-9_]{1,16}$/.test(root.typedName)
    property bool sending: false
    // "friends", "everyone" or "invisible".
    readonly property string visibility: root.controller ? String(root.controller.settings.visibility || "friends") : "friends"
    // What friends see of you: playing, worldType, host, serverHidden, joinable.
    readonly property var mine: root.controller ? root.controller.myStatus : ({})
    readonly property bool playing: !!root.mine.playing && root.visibility !== "invisible"
    // The line under your name, as friends would read you. Not while the
    // server does not answer: nothing says friends see you then.
    readonly property string statusLine: !root.online ? qsTr("Not connected · friends see your last status")
        : root.visibility === "invisible" ? qsTr("Invisible")
        : root.playing && root.mine.host ? (root.mine.joinable ? qsTr("Playing on %1 · friends can join").arg(root.mine.host)
                                                                : qsTr("Playing on %1").arg(root.mine.host))
        : root.playing && root.mine.serverHidden ? qsTr("Playing · server hidden")
        : root.playing && root.mine.worldType === "singleplayer" ? qsTr("Playing · singleplayer")
        : root.playing ? qsTr("Playing")
        : root.visibility === "everyone" ? qsTr("Online · anyone with your name can see you")
        : qsTr("Online · friends can see you")
    // Below this the add field goes under your name.
    readonly property int sideBySideWidth: 640
    readonly property int addFieldWidth: 232

    function focusField() {
        nameField.forceActiveFocus()
    }
    function submit() {
        // Enter in the field gets here too, past the Add button's own wait.
        if (!root.nameValid || !root.controller || root.sending || !root.online)
            return
        root.sending = true
        root.controller.sendRequest(root.typedName)
    }

    implicitHeight: layout.implicitHeight + Theme.space.lg * 2
    radius: Theme.radius.lg
    color: Theme.palette.surface
    border.width: 1
    border.color: Theme.palette.border

    GridLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        columns: root.width >= root.sideBySideWidth ? 2 : 1
        columnSpacing: Theme.space.xl
        rowSpacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.md

            FriendHead {
                Layout.alignment: Qt.AlignVCenter
                size: Theme.control.height
                face: root.face
                // Invisible is how you look to others: no dot, a dimmed head.
                // Not connected: no dot, as nothing says you are online.
                presence: !root.online ? ""
                        : root.visibility === "invisible" ? "offline" : root.playing ? "playing" : "online"
                ringColor: root.color
            }
            Column {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                CopyName {
                    width: Math.min(implicitWidth, parent.width)
                    value: root.controller ? root.controller.myName : ""
                    pixelSize: Theme.type.title.pixelSize
                    weight: Theme.type.title.weight
                }
                Text {
                    width: parent.width
                    text: root.statusLine
                    elide: Text.ElideRight
                    color: !root.online || root.visibility === "invisible" ? Theme.palette.textTertiary : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: root.width < root.sideBySideWidth
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            spacing: Theme.space.xs

            RowLayout {
                spacing: Theme.space.sm

                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    Layout.preferredWidth: root.addFieldWidth
                    placeholderText: qsTr("Add a friend by name")
                    maximumLength: 16
                    selectByMouse: true
                    Accessible.name: qsTr("Minecraft name of the friend to add")
                    onAccepted: root.submit()
                }
                Button {
                    highlighted: true
                    enabled: root.nameValid && !root.sending && root.online
                    text: qsTr("Add")
                    icon.source: Icons.url("plus")
                    onClicked: root.submit()
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.preferredWidth: root.addFieldWidth
                visible: root.typedName.length > 0 && !root.nameValid
                text: qsTr("A Minecraft name has 1 to 16 letters, numbers or underscores.")
                wrapMode: Text.Wrap
                color: Theme.palette.danger
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }

    // The answer to Add: only a request that went through empties the field.
    Connections {
        target: root.controller
        function onActionFinished(action, ok, message) {
            if (action !== "sendRequest")
                return
            root.sending = false
            if (ok)
                nameField.clear()
        }
        // Friends turned off and on while a request was out: its answer is
        // dropped, so it is not waited for.
        function onChanged() {
            if (root.sending && root.controller.state !== "ready")
                root.sending = false
        }
    }
}
