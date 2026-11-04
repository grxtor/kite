// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Where a message is written: "Message jeb_", Enter sends. A message is one paragraph, the way the server keeps it
 * (it removes line breaks), so the field never holds one: Shift+Enter adds none and a pasted break becomes a space.
 * It wraps and grows to a few lines, then scrolls. Near the limit (FriendsChat.maxLength) it counts what is left
 * and stops there. While the server refuses messages for sending too fast it says how long to wait. When there is
 * no one to write to it says why instead (`reason`).
 */
Item {
    id: root

    // FriendsController.chat, or null.
    property var chat: null
    property string name
    property bool canSend: true
    // Why nothing can be sent, while canSend is false.
    property string reason

    signal closeRequested()

    readonly property int maxLength: root.chat ? root.chat.maxLength : 1000
    readonly property int charsLeft: root.maxLength - field.length
    // The count shows from this many characters left.
    readonly property int countFrom: 100
    readonly property int maxLines: 6
    readonly property int maxFieldHeight: Math.ceil(field.font.pixelSize * 1.4 * root.maxLines)
                                          + field.topPadding + field.bottomPadding
    property real now: Date.now()
    readonly property int waitSeconds: root.chat && root.chat.rateLimitedUntilMs > root.now
                                       ? Math.ceil((root.chat.rateLimitedUntilMs - root.now) / 1000) : 0

    function focusField() {
        field.forceActiveFocus()
    }
    function submit() {
        if (!root.canSend || root.waitSeconds > 0 || !root.chat)
            return
        if (root.chat.send(field.text))
            field.clear()
    }
    // Line breaks become spaces and the text stops at the limit, keeping the cursor where it was.
    function tidy() {
        var text = field.text
        var flat = text.replace(/\r\n|\r|\n/g, " ")
        if (flat.length > root.maxLength)
            flat = flat.substring(0, root.maxLength)
        if (flat === text)
            return
        var cursor = Math.min(field.cursorPosition, flat.length)
        field.text = flat
        field.cursorPosition = cursor
    }

    implicitHeight: column.implicitHeight + Theme.space.md * 2

    Timer {
        running: root.waitSeconds > 0
        interval: 1000
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = Date.now()
    }
    Connections {
        target: root.chat
        ignoreUnknownSignals: true
        function onStateChanged() { root.now = Date.now() }
    }

    Column {
        id: column
        x: Theme.space.lg
        y: Theme.space.md
        width: parent.width - Theme.space.lg * 2
        spacing: Theme.space.xs

        Copy {
            width: parent.width
            visible: root.canSend && root.waitSeconds > 0
            text: qsTr("You are sending messages too fast. Wait %1 s.").arg(root.waitSeconds)
            wrapMode: Text.Wrap
            role: "caption"
            tone: "warning"
        }
        PanelNote {
            width: parent.width
            visible: !root.canSend
            text: root.reason
        }
        Flickable {
            id: scroller
            visible: root.canSend
            width: parent.width
            height: Math.min(field.implicitHeight, root.maxFieldHeight)
            contentWidth: width
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ScrollBar.vertical: ScrollBar {}

            TextArea.flickable: TextArea {
                id: field
                objectName: "chatField"
                // One line until there is more to say; the style's field is sized for a paragraph.
                background.implicitHeight: Theme.control.height
                background.implicitWidth: 0
                wrapMode: TextArea.Wrap
                placeholderText: qsTr("Message %1").arg(root.name)
                Accessible.name: qsTr("Message %1").arg(root.name)
                onTextChanged: root.tidy()
                Keys.onReturnPressed: (event) => {
                    event.accepted = true
                    if (!(event.modifiers & Qt.ShiftModifier))
                        root.submit()
                }
                Keys.onEnterPressed: (event) => {
                    event.accepted = true
                    root.submit()
                }
                Keys.onEscapePressed: root.closeRequested()
            }
        }
        Copy {
            anchors.right: parent.right
            visible: root.canSend && root.charsLeft <= root.countFrom
            text: root.charsLeft <= 0 ? qsTr("No characters left")
                : root.charsLeft === 1 ? qsTr("1 character left")
                : qsTr("%1 characters left").arg(root.charsLeft)
            role: "caption"
            tone: root.charsLeft <= 0 ? "danger" : "tertiary"
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.palette.divider
    }
}
