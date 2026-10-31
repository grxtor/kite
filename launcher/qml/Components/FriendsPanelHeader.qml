// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The head of the friends panel: its title with who is around under it, and on the right add a friend, the
 * requests waiting (count on the corner) and a button that folds the panel to its strip. The first two only show
 * while there are lists to look at.
 */
Item {
    id: root

    property bool ready: false
    property bool loaded: false
    property int friendCount: 0
    property int onlineCount: 0
    property int incomingCount: 0
    property bool showsLists: false
    property color surface: Theme.palette.canvas

    signal addRequested()
    signal requestsRequested()
    signal collapseRequested()

    // Not a word while it is not known; "No one online" only once the friends have been read.
    readonly property string subtitle: root.ready && root.onlineCount > 0 ? qsTr("%1 online").arg(root.onlineCount)
                                     : root.ready && root.loaded && root.friendCount > 0 ? qsTr("No one online")
                                     : ""

    implicitHeight: heading.implicitHeight + Theme.space.md + Theme.space.sm

    RowLayout {
        anchors.fill: parent
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
                // A non-breaking space when there is nothing to say, so the header keeps its height.
                text: root.subtitle.length > 0 ? root.subtitle : "\u00a0"
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

            CountPill {
                x: requests.width - width + Theme.space.xs
                y: -Theme.space.xs
                count: root.incomingCount
                ringColor: root.surface
            }
        }
        IconButton {
            Layout.alignment: Qt.AlignVCenter
            size: Theme.control.heightSm
            iconName: "chevron-right"
            tip: qsTr("Collapse friends")
            onClicked: root.collapseRequested()
        }
    }
}
