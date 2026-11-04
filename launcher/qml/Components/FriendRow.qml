// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One person in a list (Friends page, Home's side panel, right-edge panel): head with presence dot, name, a quiet
 * line; whatever goes inside lands on the right, in `trailing`. `presence`: "online", "playing", "offline" or ""
 * (not a friend yet). `detailTone`: "primary", "secondary" or "faint". `detailServer`: the line names a server, so
 * its favicon (`detailIcon`, a server glyph until it answers) leads it.
 */
Item {
    id: root

    property string name
    property string face
    property string presence: ""
    property string detail
    property string detailSuffix
    property string detailTone: "secondary"
    property bool detailServer: false
    property string detailIcon
    property bool compact: false
    property bool interactive: false
    readonly property bool hovered: hoverHandler.hovered
    default property alias trailing: trailingRow.data

    signal clicked()

    readonly property color fill: root.hovered || root.visualFocus ? Theme.palette.surfaceHover : Theme.palette.canvas
    readonly property int headSize: root.compact ? 32 : 40
    // The room the head takes before the text starts: compact rows keep a Theme.control.height tile so names
    // line up with Recent worlds; the right-edge panel uses the head's own width.
    property int headSlot: root.compact ? Theme.control.height : root.headSize
    // A row only takes focus from the Tab key, so focus is a keyboard ring.
    readonly property bool visualFocus: root.interactive && root.activeFocus

    implicitWidth: 280
    implicitHeight: root.headSize + Theme.space.sm * 2
    activeFocusOnTab: root.interactive

    Accessible.role: root.interactive ? Accessible.Button : Accessible.ListItem
    Accessible.name: root.detail.length > 0
                     ? qsTr("%1, %2").arg(root.name).arg(root.detail + (root.detailSuffix.length > 0 ? " " + root.detailSuffix : ""))
                     : root.name

    Keys.onReturnPressed: if (root.interactive) root.clicked()
    Keys.onSpacePressed: if (root.interactive) root.clicked()

    HoverHandler {
        id: hoverHandler
        cursorShape: root.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    TapHandler {
        enabled: root.interactive
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.clicked()
    }

    StateFill {
        anchors.fill: parent
        card: true
        rest: "transparent"
        hovered: root.hovered || root.visualFocus
        radius: Theme.radius.md

        FocusFrame { around: 0; visible: root.visualFocus }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? Theme.space.sm : Theme.space.lg
        anchors.rightMargin: root.compact ? Theme.space.sm : Theme.space.lg
        spacing: Theme.space.md

        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.headSlot
            Layout.preferredHeight: root.headSize

            FriendHead {
                anchors.verticalCenter: parent.verticalCenter
                size: root.headSize
                face: root.face
                presence: root.presence
                ringColor: root.fill
            }
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.xxs

            Copy {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                role: "bodyStrong"
                tone: root.presence === "offline" ? "secondary" : "primary"
            }

            FriendPlaceLine {
                width: parent.width
                text: root.detail
                suffix: root.detailSuffix
                tone: root.detailTone
                server: root.detailServer
                icon: root.detailIcon
            }
        }

        Row {
            id: trailingRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.xs
        }
    }
}
