// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One message in a conversation (a FriendsChat.messages row), laid out like Discord: the first of a run from one
 * side carries the head, the name and the time; the rest are text alone, their time shown in the gutter on hover.
 * The text is plain text, so "§6" stays "§6". A deleted message is a placeholder; one on its way is dimmed; one that
 * failed says why with Try again. ⋯ on hover opens the view's FriendChatMessageMenu.
 */
Item {
    id: root

    // The model's roles.
    required property string key
    required property string messageId
    required property bool mine
    required property string text
    required property bool deleted
    required property real createdAtMs
    required property string sendState
    required property string error
    required property bool groupStart
    required property bool dayStart

    property string friendName
    property string friendFace
    property string myName
    property string myFace
    // FriendChatMessageMenu, or null.
    property var menu: null

    signal retryRequested(string key)

    readonly property bool failed: root.sendState === "failed"
    readonly property bool menuOpen: !!root.menu && root.menu.visible && root.menu.key === root.key
    readonly property bool lit: hover.hovered || more.visualFocus || root.menuOpen
    readonly property int headSize: 32
    readonly property int gutter: Theme.space.lg + root.headSize + Theme.space.md
    readonly property string timeText: new Date(root.createdAtMs).toLocaleTimeString(Qt.locale(), Locale.ShortFormat)

    implicitHeight: column.implicitHeight

    HoverHandler { id: hover }

    Column {
        id: column
        width: parent.width

        FriendChatDaySeparator {
            width: parent.width
            visible: root.dayStart
            dayMs: root.createdAtMs
        }

        Item {
            id: row
            width: parent.width
            height: body.y + body.height + Theme.space.xxs

            Rectangle {
                anchors.fill: parent
                color: root.lit ? Theme.palette.surfaceHover : "transparent"
            }
            FriendHead {
                visible: root.groupStart
                x: Theme.space.lg
                y: Theme.space.sm
                size: root.headSize
                face: root.mine ? root.myFace : root.friendFace
                ringColor: Theme.palette.canvas
            }
            Copy {
                visible: !root.groupStart && root.lit
                x: Theme.space.xs
                width: root.gutter - Theme.space.xs * 2
                y: body.y + Theme.space.xxs
                horizontalAlignment: Text.AlignHCenter
                text: root.timeText
                role: "caption"
                tone: "tertiary"
            }

            Column {
                id: body
                x: root.gutter
                y: root.groupStart ? Theme.space.sm : Theme.space.xxs
                width: row.width - root.gutter - Theme.space.lg - more.width
                spacing: Theme.space.xxs

                Row {
                    visible: root.groupStart
                    width: parent.width
                    spacing: Theme.space.sm

                    Copy {
                        width: Math.min(implicitWidth, parent.width - time.implicitWidth - parent.spacing)
                        text: root.mine ? root.myName : root.friendName
                        elide: Text.ElideRight
                        role: "bodyStrong"
                    }
                    Copy {
                        id: time
                        anchors.baseline: parent.children[0].baseline
                        text: root.timeText
                        role: "caption"
                        tone: "tertiary"
                    }
                }
                Copy {
                    width: parent.width
                    text: root.deleted ? qsTr("Message deleted") : root.text
                    // Never rich text or Markdown: a friend's words are shown as written.
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.italic: root.deleted
                    tone: root.deleted || root.sendState === "sending" ? "tertiary" : "primary"
                    lineHeight: 1.2
                }
                // The reason, then Try again beside it or, when the panel is narrow, under it.
                Flow {
                    visible: root.failed
                    width: parent.width
                    spacing: Theme.space.xs

                    Row {
                        height: retry.height
                        spacing: Theme.space.xs

                        MeshIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: "alert-triangle"
                            size: Theme.icon.xxs
                            color: Theme.palette.danger
                        }
                        Copy {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, body.width - Theme.icon.xxs - Theme.space.xs)
                            text: root.error
                            elide: Text.ElideRight
                            role: "caption"
                            tone: "danger"
                        }
                    }
                    Button {
                        id: retry
                        buttonSize: "sm"
                        variant: "ghost"
                        text: qsTr("Try again")
                        onClicked: root.retryRequested(root.key)
                    }
                }
            }

            IconButton {
                id: more
                anchors.right: parent.right
                anchors.rightMargin: Theme.space.sm
                y: body.y
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                Accessible.name: qsTr("More options for this message")
                // The room stays, so text does not reflow when the pointer arrives.
                opacity: root.lit ? 1 : 0
                enabled: !!root.menu
                onClicked: root.menu.openFor(more, root)
            }
        }
    }
}
