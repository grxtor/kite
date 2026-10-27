// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The head of the friends panel: its title with who is around under it, and
 * on the right the two things you reach for from anywhere in the launcher --
 * add a friend (the Friends page, with its add field focused) and the
 * requests waiting for an answer (the page's Requests tab, with their
 * count on the corner). The page itself keeps what does not belong in a
 * narrow column: requests, blocked people and adding by name.
 *
 * The buttons only show while there are lists to look at; with friends off
 * or not connected there is nothing for either to open.
 */
Item {
    id: root

    // Friends are connected, and the lists have been read.
    property bool ready: false
    property bool loaded: false
    property int friendCount: 0
    property int onlineCount: 0
    property int incomingCount: 0
    // Whether the lists are on show (connected, or failing after they were
    // read): the buttons act on them.
    property bool showsLists: false
    // The surface the panel stands on, for the count's ring.
    property color surface: Theme.palette.canvas

    signal addRequested()
    signal requestsRequested()

    // Not a word while it is not known; "No one online" only once the
    // friends have been read and there are some.
    readonly property string subtitle: root.ready && root.onlineCount > 0 ? qsTr("%1 online").arg(root.onlineCount)
                                     : root.ready && root.loaded && root.friendCount > 0 ? qsTr("No one online")
                                     : ""

    implicitHeight: heading.implicitHeight + Theme.space.md + Theme.space.sm

    RowLayout {
        anchors.fill: parent
        // The title stands on the rows' heads and the buttons on their ⋯
        // (the panel's column inset), so the header and the list share one
        // set of edges.
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.lg
        anchors.topMargin: Theme.space.md
        anchors.bottomMargin: Theme.space.sm
        spacing: Theme.space.xs

        Column {
            id: heading
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: 0

            Text {
                width: parent.width
                text: qsTr("Friends")
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.heading
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight
                Accessible.role: Accessible.Heading
            }
            Text {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                elide: Text.ElideRight
                color: root.onlineCount > 0 ? Theme.palette.success : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            visible: root.showsLists
            size: Theme.control.heightSm
            iconName: "plus"
            tip: qsTr("Add a friend")
            onClicked: root.addRequested()
        }
        IconButton {
            id: requests
            Layout.alignment: Qt.AlignVCenter
            visible: root.showsLists
            size: Theme.control.heightSm
            iconName: "bell"
            tip: qsTr("Friend requests")
            Accessible.description: root.incomingCount > 0 ? String(root.incomingCount) : ""
            onClicked: root.requestsRequested()

            // On the corner, a little out of it, so it covers less of the glyph.
            CountPill {
                x: requests.width - width + Theme.space.xs
                y: -Theme.space.xs
                count: root.incomingCount
                ringColor: root.surface
            }
        }
    }
}
