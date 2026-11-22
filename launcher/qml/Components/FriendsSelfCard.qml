// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * You, at the top of the Friends page: head and name (a click copies it) over what friends can see of you (a big
 * server by its name and favicon, as in your friends' rows), and at the end the way to the Friends settings, where
 * that is decided.
 */
Rectangle {
    id: root

    // FriendsController, or null.
    property var controller: null
    property string face: ""
    // The friends server answers; without it a request cannot go.
    property bool online: true
    // Main.qml's FriendPlaces.
    property FriendPlaces places: FriendPlaces {}

    signal settingsRequested()

    // "friends", "everyone" or "invisible".
    readonly property string visibility: root.controller ? String(root.controller.settings.visibility || "friends") : "friends"
    // What friends see of you: playing, worldType, host, serverHidden, joinable.
    readonly property var mine: root.controller ? root.controller.myStatus : ({})
    readonly property bool playing: !!root.mine.playing && root.visibility !== "invisible"
    // The server you are on, put the way your friends' rows put theirs; ({}) when it is not shown to them.
    readonly property var place: root.online && root.playing && root.mine.host
                                 ? root.places.of({ state: "playing", worldType: "server", host: root.mine.host,
                                                    port: 0 })
                                 : ({})
    readonly property string serverName: root.place.name || root.mine.host || ""
    // Not while the server does not answer: nothing says friends see you then.
    readonly property string statusLine: !root.online ? qsTr("Not connected · friends see your last status")
        : root.visibility === "invisible" ? qsTr("Invisible")
        : root.playing && root.mine.host ? (root.mine.joinable
                                            ? qsTr("Playing on %1 · friends can join").arg(root.serverName)
                                            : qsTr("Playing on %1").arg(root.serverName))
        : root.playing && root.mine.serverHidden ? qsTr("Playing · server hidden")
        : root.playing && root.mine.worldType === "singleplayer" ? qsTr("Playing · singleplayer")
        : root.playing ? qsTr("Playing")
        : root.visibility === "everyone" ? qsTr("Online · anyone with your name can see you")
        : qsTr("Online · friends can see you")
    // As big as in a friend's card, so you and they look alike.
    readonly property int headSize: 48

    implicitHeight: layout.implicitHeight
    color: "transparent"

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.md

            FriendHead {
                Layout.alignment: Qt.AlignVCenter
                size: root.headSize
                face: root.face
                // Invisible: no dot, a dimmed head. Not connected: no dot, as nothing says you are online.
                presence: !root.online ? ""
                        : root.visibility === "invisible" ? "offline" : root.playing ? "playing" : "online"
                ringColor: Theme.palette.canvas
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
                Row {
                    width: parent.width
                    spacing: Theme.space.xs

                    ServerFavicon {
                        id: favicon
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !!root.place.address
                        source: root.place.favicon || ""
                    }
                    Copy {
                        width: parent.width - (favicon.visible ? favicon.width + parent.spacing : 0)
                        text: root.statusLine
                        elide: Text.ElideRight
                        role: "label"
                        tone: !root.online || root.visibility === "invisible" ? "tertiary" : "secondary"
                    }
                }
            }
        }

        Button {
            Layout.alignment: Qt.AlignVCenter
            flat: true
            text: qsTr("Privacy")
            icon.source: Icons.url("settings")
            Accessible.name: qsTr("Friends privacy settings")
            onClicked: root.settingsRequested()
        }
    }
}
