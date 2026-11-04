// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A conversation's messages, newest at the bottom (FriendsChat.messages is newest first, laid out bottom to top).
 * Scrolled to the top it reads older history while there is some; at the very start it says where the history
 * begins and how long it is kept. A new message keeps the view at the bottom if it was there. Loading, a failed read
 * and an empty conversation each have their own word in the middle.
 */
Item {
    id: root

    // FriendsController.chat, or null.
    property var chat: null
    // The friend as FriendsController.friends has them.
    property var friend: ({})
    property string myName
    property string myFace
    // FriendChatMessageMenu, or null.
    property var menu: null

    readonly property bool loaded: !!root.chat && root.chat.loaded
    readonly property bool empty: list.count === 0
    readonly property string name: root.friend.name || ""

    function maybeLoadOlder() {
        if (root.chat && root.chat.hasOlder && !root.chat.loadingOlder && list.atYBeginning)
            root.chat.loadOlder()
    }

    ListView {
        id: list
        anchors.fill: parent
        model: root.chat ? root.chat.messages : null
        verticalLayoutDirection: ListView.BottomToTop
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        topMargin: Theme.space.sm
        bottomMargin: Theme.space.sm
        // Bottom to top, the footer is at the top: the start of the history, or older messages on their way.
        footer: Item {
            width: list.width
            height: start.visible ? start.implicitHeight + Theme.space.lg
                  : older.visible ? older.implicitHeight + Theme.space.md : 0

            Column {
                id: start
                visible: root.loaded && !root.chat.hasOlder && !root.empty
                x: Theme.space.lg
                width: parent.width - Theme.space.lg * 2
                spacing: Theme.space.xxs

                Copy {
                    width: parent.width
                    text: qsTr("This is the start of your messages with %1.").arg(root.name)
                    wrapMode: Text.Wrap
                    role: "label"
                    tone: "secondary"
                }
                Copy {
                    width: parent.width
                    text: qsTr("Messages are kept for 30 days.")
                    wrapMode: Text.Wrap
                    role: "caption"
                    tone: "tertiary"
                }
            }
            Copy {
                id: older
                visible: !!root.chat && root.chat.loadingOlder
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Loading older messages…")
                role: "caption"
                tone: "tertiary"
            }
        }
        delegate: FriendChatMessage {
            width: list.width
            friendName: root.name
            friendFace: root.friend.face || ""
            myName: root.myName
            myFace: root.myFace
            menu: root.menu
            onRetryRequested: (key) => root.chat.retry(key)
        }

        ScrollBar.vertical: ScrollBar {}

        onAtYBeginningChanged: root.maybeLoadOlder()
        onCountChanged: Qt.callLater(root.maybeLoadOlder)

        // Bottom to top, the start of the view is its bottom.
        property bool atBottom: true
        onContentYChanged: list.atBottom = list.atYEnd
        Connections {
            target: list.model
            ignoreUnknownSignals: true
            function onRowsInserted(parent, first) {
                if (first === 0 && list.atBottom)
                    Qt.callLater(list.positionViewAtBeginning)
            }
            function onModelReset() {
                list.atBottom = true
                Qt.callLater(list.positionViewAtBeginning)
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - Theme.space.xl * 2
        visible: root.empty
        spacing: Theme.space.sm

        FriendHead {
            visible: root.loaded && root.chat.loadError.length === 0
            anchors.horizontalCenter: parent.horizontalCenter
            size: 48
            face: root.friend.face || ""
        }
        Copy {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: !root.chat ? ""
                : root.chat.loadError.length > 0 ? root.chat.loadError
                : root.chat.loading || !root.loaded ? qsTr("Loading messages…")
                : qsTr("No messages yet. Say hi to %1.").arg(root.name)
            role: "label"
            tone: !!root.chat && root.chat.loadError.length > 0 ? "danger" : "secondary"
        }
        Copy {
            width: parent.width
            visible: root.loaded && !!root.chat && root.chat.loadError.length === 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Only you two see these messages. They are kept for 30 days.")
            role: "caption"
            tone: "tertiary"
        }
        Button {
            visible: !!root.chat && root.chat.loadError.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            buttonSize: "sm"
            text: qsTr("Try again")
            icon.source: Icons.url("refresh")
            onClicked: root.chat.reload()
        }
    }
}
