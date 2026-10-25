// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * One person in a list, on the Friends page and in Home's side panel: their
 * head (FriendHead, with its presence dot), their name, and a quiet line
 * under it (where they are, or what the row is about). Whatever goes inside
 * the row (Join, a ⋯, buttons) lands on its right, in `trailing`.
 *
 * The row has no box: it sits on the page and takes the library list's
 * neutral hover fill. `compact` is Home's panel size, with its narrower
 * inset. `interactive` makes the whole row a button (`clicked()`, a focus
 * ring); on the Friends page a row is only a row, with its own buttons.
 *
 * `presence` is "online", "playing", "offline", or "" for someone who is
 * not a friend (yet). The line is `detail` and, after a dot, `detailSuffix`
 * (the game version); `detailTone` ("primary", "secondary" or "faint")
 * sets how loud `detail` is: "faint" for "Last seen", which is the least
 * of the row. A host too long for the row is shortened in the middle, so
 * its end stays and the version sits right behind it.
 */
Item {
    id: root

    property string name
    // The head's picture (FriendsController's `face`), "" until it is cached.
    property string face
    property string presence: ""
    property string detail
    property string detailSuffix
    property string detailTone: "secondary"
    property bool compact: false
    property bool interactive: false
    // The pointer is on the row; the buttons that only show then read it.
    readonly property bool hovered: hoverHandler.hovered
    default property alias trailing: trailingRow.data

    signal clicked()

    // The fill the head's ring has to match.
    readonly property color fill: root.hovered || root.visualFocus ? Theme.palette.surfaceHover : Theme.palette.canvas
    readonly property int headSize: root.compact ? 32 : 40
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

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: root.hovered || root.visualFocus ? Theme.palette.surfaceHover : "transparent"
        border.width: root.visualFocus ? Theme.focus.width : 0
        border.color: Theme.palette.focusRing
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    RowLayout {
        anchors.fill: parent
        // On the Friends page the row's head, name and buttons stand on the
        // self card's inset, so the page reads as one set of columns.
        anchors.leftMargin: root.compact ? Theme.space.sm : Theme.space.lg
        anchors.rightMargin: root.compact ? Theme.space.sm : Theme.space.lg
        spacing: Theme.space.md

        // Compact rows share Home's panel with Recent worlds, whose text
        // starts after a Theme.control.height tile: the head takes that
        // much room, so the names of the two lists line up.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.compact ? Theme.control.height : root.headSize
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

            Text {
                width: parent.width
                text: root.name
                elide: Text.ElideRight
                color: root.presence === "offline" ? Theme.palette.textSecondary : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }

            Row {
                id: detailRow
                width: parent.width
                spacing: Theme.space.xs
                visible: root.detail.length > 0

                // The elided text is worked out here, not by the Text: an
                // elided Text keeps its full box and leaves a gap before the
                // version.
                TextMetrics {
                    id: detailMetrics
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    text: root.detail
                    elide: Text.ElideMiddle
                    elideWidth: Math.max(0, detailRow.width - (suffix.visible ? suffix.implicitWidth + detailRow.spacing : 0))
                }
                Text {
                    text: detailMetrics.elidedText
                    color: root.detailTone === "primary" ? Theme.palette.textPrimary
                         : root.detailTone === "faint" ? Theme.palette.textTertiary
                         : Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Text {
                    id: suffix
                    visible: root.detailSuffix.length > 0
                    text: "· " + root.detailSuffix
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }

        Row {
            id: trailingRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.xs
        }
    }
}
