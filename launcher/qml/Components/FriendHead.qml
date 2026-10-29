// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A person's Minecraft head (the 8x8 face FriendsController cached from
 * Mojang, or the launcher's own account head; drawn pixel for pixel; a
 * neutral tile with a user icon until it is there) and, when they are
 * around, the green dot on its bottom-right corner. The dot has a ring in
 * `ringColor`, the colour the head sits on, so it reads as cut out of the
 * head rather than laid over it.
 *
 * `presence` is "online", "playing", "offline" or "" (no dot, full colour:
 * someone who is not a friend yet, or you). An offline head is dimmed. In a
 * game the dot is filled; only in Kite it is a hollow ring, so a strip of
 * heads tells the two apart without names.
 */
Item {
    id: root

    // The head's picture (FriendsController's `face`), "" until it is cached.
    property string face
    // A head is pixel art: a multiple of 8, so every texel is the same size.
    property int size: 40
    property string presence: ""
    property color ringColor: Theme.palette.canvas

    readonly property bool lit: root.presence === "online" || root.presence === "playing"
    readonly property real offlineOpacity: 0.55
    readonly property int ringWidth: 2
    // The dot grows with the head.
    readonly property int dotSize: root.size >= 40 ? 12 : 10

    implicitWidth: root.size
    implicitHeight: root.size
    Accessible.ignored: true

    Item {
        anchors.fill: parent
        opacity: root.presence === "offline" ? root.offlineOpacity : 1

        Rectangle {
            anchors.fill: parent
            visible: head.status !== Image.Ready
            color: Theme.palette.surfaceSunken

            MeshIcon {
                anchors.centerIn: parent
                iconName: "user"
                size: Theme.icon.md
                color: Theme.palette.textTertiary
            }
        }
        Image {
            id: head
            anchors.fill: parent
            source: root.face
            // The launcher's own head (image://accountface) is drawn by its
            // provider at exactly the size asked for, so every texel stays
            // whole; a cached face is already 8x8 and is left alone.
            sourceSize: root.face.indexOf("image://") === 0 ? Qt.size(root.size, root.size) : Qt.size(0, 0)
            smooth: false
            mipmap: false
            fillMode: Image.Stretch
        }
    }

    Rectangle {
        visible: root.lit
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        // Hangs a little off the corner, as a presence dot does.
        anchors.rightMargin: -Theme.space.xs
        anchors.bottomMargin: -Theme.space.xs
        width: root.dotSize + root.ringWidth * 2
        height: width
        radius: width / 2
        color: root.ringColor

        Rectangle {
            anchors.centerIn: parent
            width: root.dotSize
            height: width
            radius: width / 2
            color: root.presence === "playing" ? Theme.palette.success : root.ringColor
            border.width: root.presence === "playing" ? 0 : root.ringWidth
            border.color: Theme.palette.success
        }
    }
}
