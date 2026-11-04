// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The line above a conversation's first message of a day: "Today", "Yesterday" or the date, between two hairlines.
 */
Item {
    id: root

    // Milliseconds since the epoch of the day's first message.
    property real dayMs: 0

    readonly property int msPerDay: 86400000

    function dayText(ms) {
        var day = new Date(ms)
        day.setHours(0, 0, 0, 0)
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        var days = Math.round((today.getTime() - day.getTime()) / root.msPerDay)
        if (days === 0)
            return qsTr("Today")
        if (days === 1)
            return qsTr("Yesterday")
        return day.toLocaleDateString(Qt.locale(), "d MMMM yyyy")
    }

    implicitHeight: label.implicitHeight + Theme.space.lg

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.lg
        anchors.right: label.left
        anchors.rightMargin: Theme.space.sm
        anchors.verticalCenter: label.verticalCenter
        height: 1
        color: Theme.palette.divider
    }
    Copy {
        id: label
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: root.dayText(root.dayMs)
        role: "caption"
        tone: "tertiary"
        font.weight: Font.Medium
        Accessible.role: Accessible.Separator
    }
    Rectangle {
        anchors.left: label.right
        anchors.leftMargin: Theme.space.sm
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.lg
        anchors.verticalCenter: label.verticalCenter
        height: 1
        color: Theme.palette.divider
    }
}
