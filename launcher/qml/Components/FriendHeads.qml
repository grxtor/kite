// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The heads of a few friends side by side (FriendHead each, no presence dot), and "+N" for the rest: who is on a
 * server, in a row that has no room for names.
 */
Row {
    id: root

    // [{ name, face }].
    property var people: []
    // A multiple of 8, like every head.
    property int size: 16
    property int maxHeads: 3

    visible: root.people.length > 0
    spacing: Theme.space.xxs

    Repeater {
        model: root.people.slice(0, root.maxHeads)
        delegate: FriendHead {
            required property var modelData
            anchors.verticalCenter: parent.verticalCenter
            size: root.size
            face: modelData.face || ""
        }
    }
    Copy {
        anchors.verticalCenter: parent.verticalCenter
        visible: root.people.length > root.maxHeads
        leftPadding: Theme.space.xxs
        text: "+" + (root.people.length - root.maxHeads)
        role: "caption"
        tone: "secondary"
    }
}
