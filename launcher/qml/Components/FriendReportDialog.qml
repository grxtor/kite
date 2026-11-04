// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Reporting a message a friend sent: the message, why (spam, harassment, something else), and whether to block them
 * as well. They are not told. Kite's server keeps a copy of the message and the 20 before it for a moderator
 * (server/friends/README.md "Reports"). The answer comes back as a toast (FriendsController.actionFinished).
 */
AppDialog {
    id: root

    // FriendsController, or null.
    property var controller: null
    property string targetUuid
    property string targetName
    property string messageId
    property string messageText
    // "spam", "harassment" or "other".
    property string reason: "spam"
    readonly property int quoteLines: 4
    readonly property int quoteBarWidth: 3

    function ask(uuid, name, id, text) {
        root.targetUuid = uuid
        root.targetName = name
        root.messageId = id
        root.messageText = text
        root.reason = "spam"
        alsoBlock.checked = false
        root.open()
    }

    size: "sm"
    title: qsTr("Report this message?")
    danger: true
    primaryText: qsTr("Report")
    onPrimaryClicked: {
        if (!root.controller || !root.controller.chat)
            return
        root.controller.chat.report(root.messageId, root.reason)
        if (alsoBlock.checked)
            root.controller.block(root.targetUuid)
    }

    contentItem: Column {
        spacing: Theme.space.md

        // A quote, not a field: a bar on its left and no frame.
        Item {
            width: parent.width
            height: quote.implicitHeight

            Rectangle {
                width: root.quoteBarWidth
                height: parent.height
                radius: width / 2
                color: Theme.palette.borderStrong
            }
            Copy {
                id: quote
                x: Theme.space.md
                width: parent.width - Theme.space.md
                text: root.messageText
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                maximumLineCount: root.quoteLines
                elide: Text.ElideRight
                tone: "secondary"
            }
        }

        Copy {
            width: parent.width
            text: qsTr("What is wrong with it?")
            role: "label"
            font.weight: Font.Medium
        }
        Column {
            width: parent.width
            ButtonGroup { id: reasons }
            Repeater {
                model: [
                    { value: "spam", label: qsTr("Spam") },
                    { value: "harassment", label: qsTr("Harassment or bullying") },
                    { value: "other", label: qsTr("Something else") }
                ]
                RadioButton {
                    required property var modelData
                    ButtonGroup.group: reasons
                    text: modelData.label
                    checked: root.reason === modelData.value
                    onToggled: if (checked) root.reason = modelData.value
                }
            }
        }
        CheckBox {
            id: alsoBlock
            text: qsTr("Also block %1").arg(root.targetName)
        }
        Copy {
            width: parent.width
            text: qsTr("%1 is not told. Kite keeps a copy of this message and the ones before it so that a moderator can look at it.").arg(root.targetName)
            wrapMode: Text.Wrap
            role: "caption"
            tone: "tertiary"
            lineHeight: 1.3
        }
    }
}
